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
 * Sampling profiler on top of the forced-observation sampler.
 *
 * The sampler thread sets vm_interrupt at the profiling interval; the executor
 * records the stack of the running coroutine in its interrupt function and
 * folds identical stacks into one counter, so no PHP callback runs per sample.
 *
 * | interrupt handled within | the sample counts as                          |
 * | 2 intervals              | the stack that is running                     |
 * | later                    | idle: the thread was out of the VM (waiting   |
 * |                          | in the event loop or blocked), and the stack  |
 * |                          | that runs next did not cause the delay        |
 *
 * Frames keep references to their name strings instead of zend_function
 * pointers, because the function of a closure goes away with the closure.
 */

#include "swow_debug.h"
#include "swow_coroutine.h"

#define SWOW_DEBUG_PROFILER_MAX_DEPTH_LIMIT 1024

typedef struct swow_debug_profiler_frame_s {
    zend_string *class_name;
    zend_string *function_name;
    zend_string *filename;
    uint32_t lineno;
} swow_debug_profiler_frame_t;

typedef struct swow_debug_profiler_stack_s {
    zend_long count;
    uint32_t depth;
    swow_debug_profiler_frame_t frames[1];
} swow_debug_profiler_stack_t;

typedef struct swow_debug_profiler_s {
    /* binary key of frame string addresses and lines -> stack */
    HashTable stacks;
    uint64_t interval;
    uint32_t max_depth;
    bool running;
    uint64_t started_at;
    uint64_t stopped_at;
    zend_long samples;
    zend_long idle_samples;
    /* the frames of the sample being recorded, max_depth long */
    swow_debug_profiler_frame_t *scratch;
} swow_debug_profiler_t;

static void swow_debug_profiler_stack_dtor(zval *z_stack)
{
    swow_debug_profiler_stack_t *stack = (swow_debug_profiler_stack_t *) Z_PTR_P(z_stack);

    for (uint32_t i = 0; i < stack->depth; i++) {
        swow_debug_profiler_frame_t *frame = &stack->frames[i];
        if (frame->class_name != NULL) {
            zend_string_release(frame->class_name);
        }
        if (frame->function_name != NULL) {
            zend_string_release(frame->function_name);
        }
        if (frame->filename != NULL) {
            zend_string_release(frame->filename);
        }
    }
    efree(stack);
}

bool swow_debug_profiler_is_running(void)
{
    swow_debug_profiler_t *profiler = SWOW_DEBUG_G(profiler);
    return profiler != NULL && profiler->running;
}

void swow_debug_profiler_record(uint64_t delay)
{
    swow_debug_profiler_t *profiler = SWOW_DEBUG_G(profiler);
    swow_debug_profiler_frame_t *frames = profiler->scratch;
    uint32_t depth = 0;

    profiler->samples++;
    if (delay > profiler->interval * 2) {
        profiler->idle_samples++;
        return;
    }
    for (zend_execute_data *call = EG(current_execute_data); call != NULL && depth < profiler->max_depth; call = call->prev_execute_data) {
        zend_function *func = call->func;
        swow_debug_profiler_frame_t *frame;
        /* skip the root frame of a coroutine, an internal function without a name */
        if (func == NULL || (func->common.function_name == NULL && !ZEND_USER_CODE(func->common.type))) {
            continue;
        }
        frame = &frames[depth++];
        frame->function_name = func->common.function_name;
        frame->class_name = func->common.scope != NULL ? func->common.scope->name : NULL;
        if (ZEND_USER_CODE(func->common.type)) {
            frame->filename = func->op_array.filename;
            frame->lineno = call->opline != NULL ? call->opline->lineno : 0;
        } else {
            frame->filename = NULL;
            frame->lineno = 0;
        }
    }
    if (depth == 0) {
        profiler->idle_samples++;
        return;
    }

    /* the key is the addresses of the strings and the lines; the stack keeps
     * references to the strings, so an address can not be reused meanwhile */
    zend_string *key = zend_string_alloc(depth * (sizeof(void *) * 3 + sizeof(uint32_t)), 0);
    char *p = ZSTR_VAL(key);
    for (uint32_t i = 0; i < depth; i++) {
        memcpy(p, &frames[i].class_name, sizeof(void *)); p += sizeof(void *);
        memcpy(p, &frames[i].function_name, sizeof(void *)); p += sizeof(void *);
        memcpy(p, &frames[i].filename, sizeof(void *)); p += sizeof(void *);
        memcpy(p, &frames[i].lineno, sizeof(uint32_t)); p += sizeof(uint32_t);
    }
    *p = '\0';

    swow_debug_profiler_stack_t *stack = zend_hash_find_ptr(&profiler->stacks, key);
    if (stack == NULL) {
        stack = emalloc(sizeof(*stack) + sizeof(swow_debug_profiler_frame_t) * (depth - 1));
        stack->count = 0;
        stack->depth = depth;
        for (uint32_t i = 0; i < depth; i++) {
            swow_debug_profiler_frame_t *frame = &stack->frames[i];
            *frame = frames[i];
            if (frame->class_name != NULL) {
                zend_string_addref(frame->class_name);
            }
            if (frame->function_name != NULL) {
                zend_string_addref(frame->function_name);
            }
            if (frame->filename != NULL) {
                zend_string_addref(frame->filename);
            }
        }
        zend_hash_add_new_ptr(&profiler->stacks, key, stack);
    }
    stack->count++;
    zend_string_release(key);
}

static void swow_debug_profiler_reset(swow_debug_profiler_t *profiler)
{
    zend_hash_clean(&profiler->stacks);
    profiler->samples = 0;
    profiler->idle_samples = 0;
    profiler->started_at = uv_hrtime();
    profiler->stopped_at = 0;
}

static void swow_debug_profiler_free(swow_debug_profiler_t *profiler)
{
    zend_hash_destroy(&profiler->stacks);
    efree(profiler->scratch);
    efree(profiler);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_Swow_Debug_startProfiler, 0, 0, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, interval, IS_LONG, 0, "10")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, maxDepth, IS_LONG, 0, "64")
ZEND_END_ARG_INFO()

/* Starts over: the stacks of a previous run are discarded. */
static PHP_FUNCTION(Swow_Debug_startProfiler)
{
    swow_debug_profiler_t *profiler = SWOW_DEBUG_G(profiler);
    zend_long interval = 10;
    zend_long max_depth = 64;

    ZEND_PARSE_PARAMETERS_START(0, 2)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(interval)
        Z_PARAM_LONG(max_depth)
    ZEND_PARSE_PARAMETERS_END();

    if (UNEXPECTED(interval <= 0 || (uint64_t) interval > UINT64_MAX / 1000000)) {
        zend_argument_value_error(1, "must be a positive number of milliseconds");
        RETURN_THROWS();
    }
    if (UNEXPECTED(max_depth <= 0 || max_depth > SWOW_DEBUG_PROFILER_MAX_DEPTH_LIMIT)) {
        zend_argument_value_error(2, "must be between 1 and %d", SWOW_DEBUG_PROFILER_MAX_DEPTH_LIMIT);
        RETURN_THROWS();
    }
    if (UNEXPECTED(profiler != NULL && profiler->running)) {
        zend_throw_error(NULL, "Profiler is already running");
        RETURN_THROWS();
    }
    if (profiler != NULL) {
        swow_debug_profiler_free(profiler);
    }
    profiler = ecalloc(1, sizeof(*profiler));
    zend_hash_init(&profiler->stacks, 64, NULL, swow_debug_profiler_stack_dtor, 0);
    profiler->interval = (uint64_t) interval * 1000000;
    profiler->max_depth = (uint32_t) max_depth;
    profiler->scratch = safe_emalloc((size_t) max_depth, sizeof(swow_debug_profiler_frame_t), 0);
    swow_debug_profiler_reset(profiler);
    SWOW_DEBUG_G(profiler) = profiler;

    if (!swow_debug_sampler_start_ex(profiler->interval)) {
        SWOW_DEBUG_G(profiler) = NULL;
        swow_debug_profiler_free(profiler);
        RETURN_THROWS();
    }
    /* the sampler may already run at a shorter interval for statement hooks */
    profiler->interval = swow_debug_sampler_get_interval();
    profiler->running = true;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_Swow_Debug_stopProfiler, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

/* Stops sampling and keeps the stacks for getProfile(). */
static PHP_FUNCTION(Swow_Debug_stopProfiler)
{
    swow_debug_profiler_t *profiler = SWOW_DEBUG_G(profiler);

    ZEND_PARSE_PARAMETERS_NONE();

    if (UNEXPECTED(profiler == NULL || !profiler->running)) {
        zend_throw_error(NULL, "Profiler is not running");
        RETURN_THROWS();
    }
    profiler->running = false;
    profiler->stopped_at = uv_hrtime();
    if (cat_queue_empty(&SWOW_DEBUG_G(extended_statement_handlers))) {
        (void) swow_debug_sampler_stop();
    }
}

static int swow_debug_profiler_compare_stacks(Bucket *a, Bucket *b)
{
    zend_long count_a = ((swow_debug_profiler_stack_t *) Z_PTR(a->val))->count;
    zend_long count_b = ((swow_debug_profiler_stack_t *) Z_PTR(b->val))->count;

    return count_a < count_b ? 1 : (count_a > count_b ? -1 : 0);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_Swow_Debug_getProfile, 0, 0, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, reset, _IS_BOOL, 0, "false")
ZEND_END_ARG_INFO()

/* Stacks with their sample counts, most frequent first, frames from the
 * innermost call outwards. */
static PHP_FUNCTION(Swow_Debug_getProfile)
{
    swow_debug_profiler_t *profiler = SWOW_DEBUG_G(profiler);
    swow_debug_profiler_stack_t *stack;
    bool reset = false;
    zval z_stacks;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_BOOL(reset)
    ZEND_PARSE_PARAMETERS_END();

    array_init(return_value);
    if (profiler == NULL) {
        add_assoc_bool(return_value, "running", false);
        add_assoc_long(return_value, "intervalNs", 0);
        add_assoc_long(return_value, "durationNs", 0);
        add_assoc_long(return_value, "samples", 0);
        add_assoc_long(return_value, "idleSamples", 0);
        array_init(&z_stacks);
        add_assoc_zval(return_value, "stacks", &z_stacks);
        return;
    }

    zend_hash_sort(&profiler->stacks, swow_debug_profiler_compare_stacks, 0);
    add_assoc_bool(return_value, "running", profiler->running);
    add_assoc_long(return_value, "intervalNs", (zend_long) profiler->interval);
    add_assoc_long(return_value, "durationNs", (zend_long) ((profiler->running ? uv_hrtime() : profiler->stopped_at) - profiler->started_at));
    add_assoc_long(return_value, "samples", profiler->samples);
    add_assoc_long(return_value, "idleSamples", profiler->idle_samples);
    array_init_size(&z_stacks, zend_hash_num_elements(&profiler->stacks));
    ZEND_HASH_FOREACH_PTR(&profiler->stacks, stack) {
        zval z_stack, z_frames;
        array_init_size(&z_stack, 2);
        add_assoc_long(&z_stack, "count", stack->count);
        array_init_size(&z_frames, stack->depth);
        for (uint32_t i = 0; i < stack->depth; i++) {
            swow_debug_profiler_frame_t *frame = &stack->frames[i];
            zval z_frame;
            array_init_size(&z_frame, 4);
            if (frame->function_name == NULL) {
                add_assoc_string(&z_frame, "function", "{main}");
            } else {
                add_assoc_str(&z_frame, "function", zend_string_copy(frame->function_name));
            }
            if (frame->class_name != NULL) {
                add_assoc_str(&z_frame, "class", zend_string_copy(frame->class_name));
            }
            if (frame->filename != NULL) {
                add_assoc_str(&z_frame, "file", zend_string_copy(frame->filename));
                add_assoc_long(&z_frame, "line", frame->lineno);
            }
            add_next_index_zval(&z_frames, &z_frame);
        }
        add_assoc_zval(&z_stack, "frames", &z_frames);
        add_next_index_zval(&z_stacks, &z_stack);
    } ZEND_HASH_FOREACH_END();
    add_assoc_zval(return_value, "stacks", &z_stacks);

    if (reset) {
        swow_debug_profiler_reset(profiler);
    }
}

static const zend_function_entry swow_debug_profiler_functions[] = {
    PHP_FENTRY(Swow\\Debug\\startProfiler, PHP_FN(Swow_Debug_startProfiler), arginfo_Swow_Debug_startProfiler, 0)
    PHP_FENTRY(Swow\\Debug\\stopProfiler, PHP_FN(Swow_Debug_stopProfiler), arginfo_Swow_Debug_stopProfiler, 0)
    PHP_FENTRY(Swow\\Debug\\getProfile, PHP_FN(Swow_Debug_getProfile), arginfo_Swow_Debug_getProfile, 0)
    PHP_FE_END
};

zend_result swow_debug_profiler_module_init(int type)
{
    return zend_register_functions(NULL, swow_debug_profiler_functions, NULL, type);
}

void swow_debug_profiler_runtime_init(void)
{
    SWOW_DEBUG_G(profiler) = NULL;
}

void swow_debug_profiler_runtime_shutdown(void)
{
    swow_debug_profiler_t *profiler = SWOW_DEBUG_G(profiler);

    if (profiler != NULL) {
        SWOW_DEBUG_G(profiler) = NULL;
        swow_debug_profiler_free(profiler);
    }
}
