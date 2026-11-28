/*
  +--------------------------------------------------------------------------+
  | Swow                                                                     |
  +--------------------------------------------------------------------------+
  | Licensed under the Apache License, Version 2.0 (the "License");          |
  | you may not use this file except in compliance with the License.         |
  | You may obtain a copy of the License at                                  |
  | http://www.apache.org/licenses/LICENSE-2.0                               |
  | Unless required by applicable law or agreed to in writing, software      |
  | distributed under the License is distributed on an "AS IS" BASIS,        |
  | WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. |
  | See the License for the specific language governing permissions and      |
  | limitations under the License. See accompanying LICENSE file.            |
  +--------------------------------------------------------------------------+
  | Author: Twosee <twosee@php.net>                                          |
  +--------------------------------------------------------------------------+
 */

/*
 * Runtime data for machines: what each coroutine waits for, the resources of
 * the thread (connections and channels) and the state of the event loop.
 *
 * Everything here runs on the thread that owns the data and returns plain
 * arrays, so a caller can json_encode() the result directly. Assembling a full
 * snapshot or an architecture graph is left to the debugger (swow-library).
 *
 * Resource ids are strings: "socket:<id>" and "channel:<id>".
 */

#include "swow_debug.h"
#include "swow_coroutine.h"
#include "swow_channel.h"

#include "cat_socket.h"
#include "cat_channel.h"
#include "cat_process.h"
#include "cat_event.h"
#include "cat_work.h"
#include "cat_poll.h"

/* resources */

static void swow_debug_add_address(zval *z_array, const char *key, const cat_sockaddr_info_t *info)
{
    char name[CAT_SOCKADDR_MAX_PATH + 1];
    size_t name_length = sizeof(name);
    int port = 0;

    if (info == NULL || info->length == 0 ||
        cat_sockaddr_to_name_silent(&info->address.common, info->length, name, &name_length, &port) != 0) {
        add_assoc_null(z_array, key);
        return;
    }
    if (port <= 0) {
        add_assoc_stringl(z_array, key, name, name_length);
    } else if (memchr(name, ':', name_length) != NULL) {
        add_assoc_str(z_array, key, zend_strpprintf(0, "[%.*s]:%d", (int) name_length, name, port));
    } else {
        add_assoc_str(z_array, key, zend_strpprintf(0, "%.*s:%d", (int) name_length, name, port));
    }
}

static zend_string *swow_debug_socket_resource_id(const cat_socket_internal_t *socket_i)
{
    return zend_strpprintf(0, "socket:" CAT_SOCKET_ID_FMT, socket_i->resource.id);
}

static zend_string *swow_debug_channel_resource_id(const swow_channel_t *s_channel)
{
    return zend_strpprintf(0, "channel:" ZEND_LONG_FMT, s_channel->id);
}

/* Only Swow\Channel objects carry a resource id; other users of cat_channel
 * (the mongodb locks) do not install the zval destructor. */
static swow_channel_t *swow_debug_get_swow_channel(const cat_channel_t *channel)
{
    if (channel->dtor != (cat_channel_data_dtor_t) zval_ptr_dtor) {
        return NULL;
    }
    return swow_channel_get_from_handle((cat_channel_t *) channel);
}

/* Fills the description of a connection, without waiters and owners. */
static void swow_debug_fill_socket_resource(zval *z_resource, cat_socket_internal_t *socket_i)
{
    cat_socket_t *socket = cat_queue_front_data(&socket_i->sockets, cat_socket_t, node);
    uint64_t now = uv_now(&CAT_EVENT_G(loop));

    array_init(z_resource);
    add_assoc_str(z_resource, "id", swow_debug_socket_resource_id(socket_i));
    add_assoc_string(z_resource, "kind", "socket");
    add_assoc_string(z_resource, "type", cat_socket_type_get_name(socket_i->type));
    if (socket_i->resource.label != NULL) {
        add_assoc_string(z_resource, "label", socket_i->resource.label);
    } else {
        add_assoc_null(z_resource, "label");
    }
    if (socket != NULL) {
        add_assoc_long(z_resource, "fd", (zend_long) cat_socket_get_fd_fast(socket));
        /* the names are cached by libcat, so only the first query costs a syscall */
        swow_debug_add_address(z_resource, "local", cat_socket_getsockname_fast(socket));
        swow_debug_add_address(z_resource, "peer", cat_socket_getpeername_fast(socket));
        add_assoc_string(z_resource, "state", cat_socket_is_established(socket) ? "established" : (
            (socket_i->flags & CAT_SOCKET_INTERNAL_FLAG_SERVER) ? "listening" : "open"));
    } else {
        add_assoc_long(z_resource, "fd", -1);
        add_assoc_null(z_resource, "local");
        add_assoc_null(z_resource, "peer");
        add_assoc_string(z_resource, "state", "closing");
    }
    add_assoc_string(z_resource, "ioState", cat_socket_io_state_name(socket_i->io_flags));
    add_assoc_long(z_resource, "ageMs", (zend_long) (now >= socket_i->resource.created_at ? now - socket_i->resource.created_at : 0));
    add_assoc_long(z_resource, "bytesIn", (zend_long) socket_i->resource.bytes_in);
    add_assoc_long(z_resource, "bytesOut", (zend_long) socket_i->resource.bytes_out);
}

static void swow_debug_fill_channel_resource(zval *z_resource, swow_channel_t *s_channel)
{
    cat_channel_t *channel = &s_channel->channel;

    array_init(z_resource);
    add_assoc_str(z_resource, "id", swow_debug_channel_resource_id(s_channel));
    add_assoc_string(z_resource, "kind", "channel");
    if (s_channel->label != NULL) {
        add_assoc_str(z_resource, "label", zend_string_copy(s_channel->label));
    } else {
        add_assoc_null(z_resource, "label");
    }
    add_assoc_long(z_resource, "capacity", (zend_long) cat_channel_get_capacity(channel));
    add_assoc_long(z_resource, "length", (zend_long) cat_channel_get_length(channel));
    add_assoc_bool(z_resource, "closed", !cat_channel_is_available(channel));
}

/* wait */

static const char *swow_debug_wait_op_name(const cat_coroutine_wait_t *wait)
{
    switch ((cat_coroutine_wait_kind_t) wait->kind) {
        case CAT_COROUTINE_WAIT_KIND_SOCKET:
            return cat_socket_io_state_name((cat_socket_io_flags_t) wait->op);
        case CAT_COROUTINE_WAIT_KIND_CHANNEL:
            switch (wait->op) {
                case CAT_COROUTINE_WAIT_OP_CHANNEL_PUSH: return "push";
                case CAT_COROUTINE_WAIT_OP_CHANNEL_POP: return "pop";
                case CAT_COROUTINE_WAIT_OP_CHANNEL_SELECT: return "select";
            }
            return NULL;
        case CAT_COROUTINE_WAIT_KIND_POLL:
            if ((wait->op & POLLIN) && (wait->op & POLLOUT)) {
                return "readWrite";
            }
            return (wait->op & POLLOUT) ? "write" : ((wait->op & POLLIN) ? "read" : NULL);
        case CAT_COROUTINE_WAIT_KIND_DNS:
            return wait->op == CAT_COROUTINE_WAIT_OP_DNS_GETNAMEINFO ? "getnameinfo" : "getaddrinfo";
        case CAT_COROUTINE_WAIT_KIND_FS:
            /* the operation name of the fs module, e.g. "open" */
            return wait->detail;
        case CAT_COROUTINE_WAIT_KIND_WORK:
            switch (wait->op) {
                case CAT_WORK_KIND_CPU: return "cpu";
                case CAT_WORK_KIND_FAST_IO: return "fastIo";
                case CAT_WORK_KIND_SLOW_IO: return "slowIo";
            }
            return NULL;
        default:
            return NULL;
    }
}

/* Fills z_object with the wait target and returns its resource id, if any. */
static zend_string *swow_debug_fill_wait_object(zval *z_object, const cat_coroutine_wait_t *wait)
{
    array_init(z_object);
    switch ((cat_coroutine_wait_kind_t) wait->kind) {
        case CAT_COROUTINE_WAIT_KIND_SOCKET: {
            cat_socket_internal_t *socket_i = (cat_socket_internal_t *) wait->object;
            zval_ptr_dtor(z_object);
            swow_debug_fill_socket_resource(z_object, socket_i);
            return swow_debug_socket_resource_id(socket_i);
        }
        case CAT_COROUTINE_WAIT_KIND_CHANNEL: {
            if (wait->op == CAT_COROUTINE_WAIT_OP_CHANNEL_SELECT) {
                const cat_channel_select_request_t *requests = (const cat_channel_select_request_t *) wait->object;
                zval z_channels;
                array_init(&z_channels);
                for (int32_t i = 0; i < wait->data; i++) {
                    swow_channel_t *s_channel = swow_debug_get_swow_channel(requests[i].channel);
                    if (s_channel != NULL) {
                        add_next_index_str(&z_channels, swow_debug_channel_resource_id(s_channel));
                    }
                }
                add_assoc_zval(z_object, "channels", &z_channels);
                return NULL;
            }
            swow_channel_t *s_channel = swow_debug_get_swow_channel((const cat_channel_t *) wait->object);
            if (s_channel == NULL) {
                return NULL;
            }
            zval_ptr_dtor(z_object);
            swow_debug_fill_channel_resource(z_object, s_channel);
            return swow_debug_channel_resource_id(s_channel);
        }
        case CAT_COROUTINE_WAIT_KIND_POLL:
            if (wait->data >= 0) {
                add_assoc_long(z_object, "fd", wait->data);
            } else {
                add_assoc_long(z_object, "fdCount", -(zend_long) wait->data);
            }
            break;
        case CAT_COROUTINE_WAIT_KIND_PROCESS:
            add_assoc_long(z_object, "pid", (zend_long) cat_process_get_pid((const cat_process_t *) wait->object));
            break;
        case CAT_COROUTINE_WAIT_KIND_SIGNAL:
            add_assoc_long(z_object, "signal", wait->data);
            break;
        case CAT_COROUTINE_WAIT_KIND_DNS:
            if (wait->detail != NULL) {
                add_assoc_string(z_object, "host", wait->detail);
            } else {
                add_assoc_null(z_object, "host");
            }
            break;
        case CAT_COROUTINE_WAIT_KIND_FS: {
            const uv_fs_t *fs = (const uv_fs_t *) wait->object;
            if (fs->path != NULL) {
                add_assoc_string(z_object, "path", fs->path);
            } else {
                add_assoc_null(z_object, "path");
            }
            break;
        }
        default:
            break;
    }
    return NULL;
}

SWOW_API void swow_debug_get_coroutine_wait(const swow_coroutine_t *s_coroutine, zval *z_wait)
{
    const cat_coroutine_t *coroutine = &s_coroutine->coroutine;
    const cat_coroutine_wait_t *wait;

    if (coroutine->state != CAT_COROUTINE_STATE_WAITING || coroutine == CAT_COROUTINE_G(scheduler)) {
        ZVAL_NULL(z_wait);
        return;
    }
    array_init(z_wait);
    wait = cat_coroutine_get_wait(coroutine);
    if (wait == NULL) {
        /* suspended by a plain yield (or by a module without a description),
         * the stack tells what it waits for */
        add_assoc_string(z_wait, "kind", "none");
        add_assoc_null(z_wait, "op");
        add_assoc_null(z_wait, "durationNs");
        add_assoc_null(z_wait, "timeoutNs");
        add_assoc_null(z_wait, "resourceId");
        add_assoc_null(z_wait, "object");
        return;
    }
    const char *op = swow_debug_wait_op_name(wait);
    zval z_object;
    zend_string *resource_id;

    add_assoc_string(z_wait, "kind", cat_coroutine_wait_kind_name((cat_coroutine_wait_kind_t) wait->kind));
    if (op != NULL) {
        add_assoc_string(z_wait, "op", op);
    } else {
        add_assoc_null(z_wait, "op");
    }
    add_assoc_long(z_wait, "durationNs", (zend_long) (uv_hrtime() - wait->since));
    if (wait->timeout < 0) {
        add_assoc_null(z_wait, "timeoutNs");
    } else {
        add_assoc_long(z_wait, "timeoutNs", (zend_long) wait->timeout * 1000 * 1000);
    }
    resource_id = swow_debug_fill_wait_object(&z_object, wait);
    if (zend_hash_num_elements(Z_ARRVAL(z_object)) == 0) {
        /* nothing to describe beyond the kind (sleep, wait group, ...) */
        zval_ptr_dtor(&z_object);
        ZVAL_NULL(&z_object);
    }
    if (resource_id != NULL) {
        add_assoc_str(z_wait, "resourceId", resource_id);
    } else {
        add_assoc_null(z_wait, "resourceId");
    }
    add_assoc_zval(z_wait, "object", &z_object);
}

/* Swow\Debug functions */

static void swow_debug_fill_coroutine_summary(zval *z_summary, swow_coroutine_t *s_coroutine)
{
    cat_coroutine_t *coroutine = &s_coroutine->coroutine;
    const char *role = cat_coroutine_get_role_name(coroutine);
    zval z_tmp;

    array_init(z_summary);
    add_assoc_long(z_summary, "id", (zend_long) coroutine->id);
    if (role != NULL) {
        add_assoc_string(z_summary, "role", role);
    } else {
        add_assoc_null(z_summary, "role");
    }
    add_assoc_string(z_summary, "state", cat_coroutine_get_state_name(coroutine));
    add_assoc_long(z_summary, "parentId", (zend_long) cat_coroutine_get_parent_id(coroutine));
    if (s_coroutine->label != NULL) {
        add_assoc_str(z_summary, "label", zend_string_copy(s_coroutine->label));
    } else {
        add_assoc_null(z_summary, "label");
    }
    add_assoc_long(z_summary, "switches", (zend_long) coroutine->switches);
    add_assoc_long(z_summary, "startedAt", (zend_long) cat_coroutine_get_start_time(coroutine));
    add_assoc_long(z_summary, "elapsedMs", (zend_long) cat_coroutine_get_elapsed(coroutine));
    if (cat_coroutine_get_busy_accounting()) {
        add_assoc_long(z_summary, "busyNs", (zend_long) cat_coroutine_get_busy_ns(coroutine));
    } else {
        add_assoc_null(z_summary, "busyNs");
    }
    swow_coroutine_get_created_location(s_coroutine, &z_tmp);
    add_assoc_zval(z_summary, "createdBy", &z_tmp);
    swow_debug_get_coroutine_wait(s_coroutine, &z_tmp);
    add_assoc_zval(z_summary, "wait", &z_tmp);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_Swow_Debug_getCoroutineSummaries, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

/* All coroutines without their stacks: the cost per coroutine is a few small
 * arrays, so a full listing stays cheap with tens of thousands of them. */
static PHP_FUNCTION(Swow_Debug_getCoroutineSummaries)
{
    zval *z_coroutine;

    ZEND_PARSE_PARAMETERS_NONE();

    array_init_size(return_value, zend_hash_num_elements(SWOW_COROUTINE_G(map)));
    ZEND_HASH_FOREACH_VAL(SWOW_COROUTINE_G(map), z_coroutine) {
        zval z_summary;
        swow_debug_fill_coroutine_summary(&z_summary, swow_coroutine_get_from_object(Z_OBJ_P(z_coroutine)));
        add_next_index_zval(return_value, &z_summary);
    } ZEND_HASH_FOREACH_END();
}

/* waiter lists keyed by the address of the waited object */
static void swow_debug_collect_waiters(HashTable *waiters)
{
    zval *z_coroutine;

    ZEND_HASH_FOREACH_VAL(SWOW_COROUTINE_G(map), z_coroutine) {
        cat_coroutine_t *coroutine = &swow_coroutine_get_from_object(Z_OBJ_P(z_coroutine))->coroutine;
        const cat_coroutine_wait_t *wait = cat_coroutine_get_wait(coroutine);
        if (wait == NULL || wait->object == NULL) {
            continue;
        }
        if (wait->kind != CAT_COROUTINE_WAIT_KIND_SOCKET && wait->kind != CAT_COROUTINE_WAIT_KIND_CHANNEL) {
            continue;
        }
        if (wait->kind == CAT_COROUTINE_WAIT_KIND_CHANNEL && wait->op == CAT_COROUTINE_WAIT_OP_CHANNEL_SELECT) {
            const cat_channel_select_request_t *requests = (const cat_channel_select_request_t *) wait->object;
            for (int32_t i = 0; i < wait->data; i++) {
                zend_ulong key = (zend_ulong) (uintptr_t) requests[i].channel;
                zval *z_list = zend_hash_index_find(waiters, key);
                if (z_list == NULL) {
                    zval z_new;
                    array_init(&z_new);
                    z_list = zend_hash_index_add_new(waiters, key, &z_new);
                }
                add_next_index_long(z_list, (zend_long) coroutine->id);
            }
            continue;
        }
        zend_ulong key = (zend_ulong) (uintptr_t) wait->object;
        zval *z_list = zend_hash_index_find(waiters, key);
        if (z_list == NULL) {
            zval z_new;
            array_init(&z_new);
            z_list = zend_hash_index_add_new(waiters, key, &z_new);
        }
        add_next_index_long(z_list, (zend_long) coroutine->id);
    } ZEND_HASH_FOREACH_END();
}

static void swow_debug_add_waiters(zval *z_resource, HashTable *waiters, const void *object)
{
    zval *z_list = zend_hash_index_find(waiters, (zend_ulong) (uintptr_t) object);

    if (z_list != NULL) {
        Z_ADDREF_P(z_list);
        add_assoc_zval(z_resource, "waiters", z_list);
    } else {
        zval z_empty;
        array_init(&z_empty);
        add_assoc_zval(z_resource, "waiters", &z_empty);
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_Swow_Debug_getResources, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_FUNCTION(Swow_Debug_getResources)
{
    HashTable waiters;

    ZEND_PARSE_PARAMETERS_NONE();

    zend_hash_init(&waiters, 8, NULL, ZVAL_PTR_DTOR, 0);
    swow_debug_collect_waiters(&waiters);

    array_init(return_value);
    CAT_QUEUE_FOREACH_DATA_START(cat_socket_get_registry(), cat_socket_internal_t, resource.node, socket_i) {
        zval z_resource, z_owners;
        swow_debug_fill_socket_resource(&z_resource, socket_i);
        swow_debug_add_waiters(&z_resource, &waiters, socket_i);
        /* the coroutine that used the connection last, an approximation of
         * who holds it (a pooled connection has no explicit owner) */
        array_init(&z_owners);
        if (socket_i->resource.last_coroutine_id != 0) {
            add_next_index_long(&z_owners, (zend_long) socket_i->resource.last_coroutine_id);
        }
        add_assoc_zval(&z_resource, "owners", &z_owners);
        add_next_index_zval(return_value, &z_resource);
    } CAT_QUEUE_FOREACH_DATA_END();
    CAT_QUEUE_FOREACH_DATA_START(&SWOW_G(observe.channels), swow_channel_t, resource_node, s_channel) {
        zval z_resource, z_owners;
        swow_debug_fill_channel_resource(&z_resource, s_channel);
        swow_debug_add_waiters(&z_resource, &waiters, &s_channel->channel);
        array_init(&z_owners);
        add_assoc_zval(&z_resource, "owners", &z_owners);
        add_next_index_zval(return_value, &z_resource);
    } CAT_QUEUE_FOREACH_DATA_END();

    zend_hash_destroy(&waiters);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_Swow_Debug_setResourceLabel, 0, 2, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, resourceId, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, label, IS_STRING, 1)
ZEND_END_ARG_INFO()

/* Labels a resource found in getResources(), for connections that have no
 * Socket object to call setLabel() on (PDO and other stream users). */
static PHP_FUNCTION(Swow_Debug_setResourceLabel)
{
    zend_string *resource_id;
    zend_string *label;
    zend_long id;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STR(resource_id)
        Z_PARAM_STR_OR_NULL(label)
    ZEND_PARSE_PARAMETERS_END();

    if (sscanf(ZSTR_VAL(resource_id), "socket:" ZEND_LONG_FMT, &id) == 1) {
        CAT_QUEUE_FOREACH_DATA_START(cat_socket_get_registry(), cat_socket_internal_t, resource.node, socket_i) {
            if ((zend_long) socket_i->resource.id == id) {
                if (socket_i->resource.label != NULL) {
                    cat_free(socket_i->resource.label);
                }
                socket_i->resource.label = label != NULL ? cat_strdup(ZSTR_VAL(label)) : NULL;
                return;
            }
        } CAT_QUEUE_FOREACH_DATA_END();
    } else if (sscanf(ZSTR_VAL(resource_id), "channel:" ZEND_LONG_FMT, &id) == 1) {
        CAT_QUEUE_FOREACH_DATA_START(&SWOW_G(observe.channels), swow_channel_t, resource_node, s_channel) {
            if (s_channel->id == id) {
                swow_channel_set_label(s_channel, label);
                return;
            }
        } CAT_QUEUE_FOREACH_DATA_END();
    } else {
        zend_argument_value_error(1, "must be \"socket:<id>\" or \"channel:<id>\"");
        RETURN_THROWS();
    }

    zend_argument_value_error(1, "refers to no live resource");
    RETURN_THROWS();
}

/* event loop */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_Swow_Debug_getEventLoopMetrics, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static PHP_FUNCTION(Swow_Debug_getEventLoopMetrics)
{
    cat_event_metrics_t metrics;
    uint32_t counts[UV_HANDLE_TYPE_MAX];
    zval z_handles;

    ZEND_PARSE_PARAMETERS_NONE();

    cat_event_get_metrics(&metrics);
    cat_event_count_handles(counts);

    array_init(return_value);
    add_assoc_long(return_value, "round", (zend_long) metrics.round);
    add_assoc_long(return_value, "loopCount", (zend_long) metrics.loop_count);
    add_assoc_long(return_value, "events", (zend_long) metrics.events);
    add_assoc_long(return_value, "eventsWaiting", (zend_long) metrics.events_waiting);
    add_assoc_long(return_value, "idleTimeNs", (zend_long) metrics.idle_time_ns);
    add_assoc_long(return_value, "activeHandles", (zend_long) metrics.active_handles);
    add_assoc_long(return_value, "activeRequests", (zend_long) metrics.active_requests);
    array_init(&z_handles);
    for (int type = UV_UNKNOWN_HANDLE + 1; type < UV_HANDLE_TYPE_MAX; type++) {
        const char *name = uv_handle_type_name((uv_handle_type) type);
        if (counts[type] != 0 && name != NULL) {
            add_assoc_long(&z_handles, name, (zend_long) counts[type]);
        }
    }
    add_assoc_zval(return_value, "handlesByType", &z_handles);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_Swow_Debug_startEventLoopDelayMonitor, 0, 0, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, resolution, IS_LONG, 0, "10")
ZEND_END_ARG_INFO()

static PHP_FUNCTION(Swow_Debug_startEventLoopDelayMonitor)
{
    zend_long resolution = 10;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(resolution)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(!cat_event_delay_monitor_start((cat_timeout_t) resolution))) {
        swow_throw_exception_with_last(swow_exception_ce);
        RETURN_THROWS();
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_Swow_Debug_stopEventLoopDelayMonitor, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

static PHP_FUNCTION(Swow_Debug_stopEventLoopDelayMonitor)
{
    ZEND_PARSE_PARAMETERS_NONE();

    if (UNEXPECTED(!cat_event_delay_monitor_stop())) {
        swow_throw_exception_with_last(swow_exception_ce);
        RETURN_THROWS();
    }
}

/* the upper bound of the bucket that holds the given rank */
static uint64_t swow_debug_delay_percentile(const cat_event_delay_stats_t *stats, double percentile)
{
    uint64_t rank = (uint64_t) (stats->count * percentile);
    uint64_t seen = 0;

    for (int i = 0; i < CAT_EVENT_DELAY_BUCKET_COUNT; i++) {
        seen += stats->buckets[i];
        if (seen > rank) {
            uint64_t upper = ((uint64_t) 2 << i) * 1000;
            return upper < stats->max_ns ? upper : stats->max_ns;
        }
    }
    return stats->max_ns;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_Swow_Debug_getEventLoopDelay, 0, 0, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, reset, _IS_BOOL, 0, "false")
ZEND_END_ARG_INFO()

/* Percentiles are the upper bounds of power-of-two microsecond buckets, so
 * they are approximations within a factor of two. */
static PHP_FUNCTION(Swow_Debug_getEventLoopDelay)
{
    cat_event_delay_stats_t stats;
    bool reset = false;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_BOOL(reset)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(!cat_event_delay_monitor_get_stats(&stats, reset))) {
        swow_throw_exception_with_last(swow_exception_ce);
        RETURN_THROWS();
    }

    array_init(return_value);
    add_assoc_long(return_value, "resolutionNs", (zend_long) stats.resolution_ns);
    add_assoc_long(return_value, "count", (zend_long) stats.count);
    add_assoc_long(return_value, "minNs", (zend_long) stats.min_ns);
    add_assoc_long(return_value, "maxNs", (zend_long) stats.max_ns);
    add_assoc_long(return_value, "meanNs", stats.count != 0 ? (zend_long) (stats.sum_ns / stats.count) : 0);
    add_assoc_long(return_value, "lastNs", (zend_long) stats.last_ns);
    add_assoc_long(return_value, "p50Ns", (zend_long) swow_debug_delay_percentile(&stats, 0.50));
    add_assoc_long(return_value, "p90Ns", (zend_long) swow_debug_delay_percentile(&stats, 0.90));
    add_assoc_long(return_value, "p99Ns", (zend_long) swow_debug_delay_percentile(&stats, 0.99));
}

static const zend_function_entry swow_debug_observe_functions[] = {
    PHP_FENTRY(Swow\\Debug\\getCoroutineSummaries, PHP_FN(Swow_Debug_getCoroutineSummaries), arginfo_Swow_Debug_getCoroutineSummaries, 0)
    PHP_FENTRY(Swow\\Debug\\getResources, PHP_FN(Swow_Debug_getResources), arginfo_Swow_Debug_getResources, 0)
    PHP_FENTRY(Swow\\Debug\\setResourceLabel, PHP_FN(Swow_Debug_setResourceLabel), arginfo_Swow_Debug_setResourceLabel, 0)
    PHP_FENTRY(Swow\\Debug\\getEventLoopMetrics, PHP_FN(Swow_Debug_getEventLoopMetrics), arginfo_Swow_Debug_getEventLoopMetrics, 0)
    PHP_FENTRY(Swow\\Debug\\startEventLoopDelayMonitor, PHP_FN(Swow_Debug_startEventLoopDelayMonitor), arginfo_Swow_Debug_startEventLoopDelayMonitor, 0)
    PHP_FENTRY(Swow\\Debug\\stopEventLoopDelayMonitor, PHP_FN(Swow_Debug_stopEventLoopDelayMonitor), arginfo_Swow_Debug_stopEventLoopDelayMonitor, 0)
    PHP_FENTRY(Swow\\Debug\\getEventLoopDelay, PHP_FN(Swow_Debug_getEventLoopDelay), arginfo_Swow_Debug_getEventLoopDelay, 0)
    PHP_FE_END
};

zend_result swow_debug_observe_module_init(int type)
{
    return zend_register_functions(NULL, swow_debug_observe_functions, NULL, type);
}

void swow_debug_observe_runtime_init(void)
{
    cat_queue_init(&SWOW_G(observe.channels));
    SWOW_G(observe.last_channel_id) = 0;
}
