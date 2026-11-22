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
  | Author: Yun Dou <dixyes@gmail.com>                                       |
  +--------------------------------------------------------------------------+
 */

#include "swow_dns_private.h"
#include "swow_hook.h"

#include "cat_work.h"

#ifndef MAXFQDNLEN
# define MAXFQDNLEN 255
#endif

static PHP_FUNCTION_EX(_swow_gethostbyname, bool is_v2)
{
    char *hostname;
    size_t hostname_len;
    zend_long af = AF_INET;

    ZEND_PARSE_PARAMETERS_START(1, (is_v2 ? 2 : 1))
        Z_PARAM_PATH(hostname, hostname_len)
        if (is_v2) {
            Z_PARAM_OPTIONAL
            Z_PARAM_LONG(af)
        }
    ZEND_PARSE_PARAMETERS_END();

    if (hostname_len > MAXFQDNLEN) {
        if (is_v2) {
            zend_argument_value_error(1, "Host name cannot be longer than %d characters", MAXFQDNLEN);
            RETURN_THROWS();
        } else {
            /* name too long, protect from CVE-2015-0235 */
            php_error_docref(NULL, E_WARNING, "Host name cannot be longer than %d characters", MAXFQDNLEN);
            RETURN_STRINGL(hostname, hostname_len);
        }
    }

    zend_string *buffer = zend_string_alloc(af == AF_INET ? CAT_SOCKET_IPV4_BUFFER_SIZE : CAT_SOCKET_IPV6_BUFFER_SIZE, false);
    cat_bool_t ret = cat_dns_get_ip(ZSTR_VAL(buffer), ZSTR_LEN(buffer), hostname, af);
    if (!ret) {
        zend_string_release_ex(buffer, false);
        if (is_v2) {
            swow_throw_exception_with_last_as_reason(spl_ce_RuntimeException, "Failed get address info for host \"%s\"", hostname);
            RETURN_THROWS();
        } else {
            RETURN_STRINGL(hostname, hostname_len);
        }
    }
    ZSTR_VAL(buffer)[ZSTR_LEN(buffer) = strlen(ZSTR_VAL(buffer))] = '\0';

    RETURN_STR(buffer);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_gethostbyname, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, hostname, IS_STRING, 0)
ZEND_END_ARG_INFO()

static PHP_FUNCTION(swow_gethostbyname)
{
    PHP_FUNCTION_CALL(_swow_gethostbyname, false);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_swow_gethostbyname2, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, hostname, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, address_family, IS_LONG, 0, "AF_INET")
ZEND_END_ARG_INFO()

static PHP_FUNCTION(swow_gethostbyname2)
{
    PHP_FUNCTION_CALL(_swow_gethostbyname, true);
}

/* {{{ gethostbynamel() */

/* PHP resolves gethostbynamel() with php_network_gethostbyname(): gethostbyname_r() where it exists,
 * otherwise gethostbyname(). The same call runs in the thread pool here, so it is only hooked where
 * that call is thread-safe:
 *
 * | Platform                        | resolver in the thread pool            |
 * | HAVE_GETHOSTBYNAME_R (3/5/6)    | gethostbyname_r(), as PHP              |
 * | Darwin, Windows                 | gethostbyname(), thread-specific data  |
 * | others                          | not hooked, native and blocking        |
 */
#if defined(HAVE_GETHOSTBYNAME_R) || defined(__APPLE__) || defined(PHP_WIN32)
# define SWOW_DNS_HAVE_GETHOSTBYNAMEL 1
#endif

#ifdef SWOW_DNS_HAVE_GETHOSTBYNAMEL

#ifdef HAVE_GETHOSTBYNAME_R
// from main/network.c @ 34ea00fda3045c39e10683465ec552e64f47f4c0, with the system allocator for workers
#ifdef HAVE_FUNC_GETHOSTBYNAME_R_6
static struct hostent *swow_gethostname_re(const char *host, struct hostent *hostbuf, char **tmphstbuf, size_t *hstbuflen)
{
    struct hostent *hp;
    int herr, res;

    if (*hstbuflen == 0) {
        *hstbuflen = 1024;
        *tmphstbuf = (char *) cat_sys_malloc(*hstbuflen);
    }

    while ((res =
        gethostbyname_r(host, hostbuf, *tmphstbuf, *hstbuflen, &hp, &herr))
        && (errno == ERANGE)) {
        /* Enlarge the buffer. */
        *hstbuflen *= 2;
        *tmphstbuf = (char *) cat_sys_realloc(*tmphstbuf, *hstbuflen);
    }

    if (res != 0) {
        return NULL;
    }

    return hp;
}
#endif
#ifdef HAVE_FUNC_GETHOSTBYNAME_R_5
static struct hostent *swow_gethostname_re(const char *host, struct hostent *hostbuf, char **tmphstbuf, size_t *hstbuflen)
{
    struct hostent *hp;
    int herr;

    if (*hstbuflen == 0) {
        *hstbuflen = 1024;
        *tmphstbuf = (char *) cat_sys_malloc(*hstbuflen);
    }

    while ((NULL == (hp =
        gethostbyname_r(host, hostbuf, *tmphstbuf, *hstbuflen, &herr)))
        && (errno == ERANGE)) {
        /* Enlarge the buffer. */
        *hstbuflen *= 2;
        *tmphstbuf = (char *) cat_sys_realloc(*tmphstbuf, *hstbuflen);
    }
    return hp;
}
#endif
#ifdef HAVE_FUNC_GETHOSTBYNAME_R_3
static struct hostent *swow_gethostname_re(const char *host, struct hostent *hostbuf, char **tmphstbuf, size_t *hstbuflen)
{
    if (*hstbuflen == 0) {
        *hstbuflen = sizeof(struct hostent_data);
        *tmphstbuf = (char *) cat_sys_malloc(*hstbuflen);
    } else {
        if (*hstbuflen < sizeof(struct hostent_data)) {
            *hstbuflen = sizeof(struct hostent_data);
            *tmphstbuf = (char *) cat_sys_realloc(*tmphstbuf, *hstbuflen);
        }
    }
    memset((void *) (*tmphstbuf), 0, *hstbuflen);

    if (0 != gethostbyname_r(host, hostbuf, (struct hostent_data *) *tmphstbuf)) {
        return NULL;
    }

    return hostbuf;
}
#endif
#endif /* HAVE_GETHOSTBYNAME_R */

/* Everything a worker thread touches is owned by this job and comes from cat_sys_malloc():
 * cat_malloc() is emalloc() in Swow, which only the PHP thread may use. The waiting coroutine
 * may also give up (timeout or cancellation) while the resolver is still
 * running; cat_work() then frees the job once the worker has finished. */
typedef struct swow_gethostbyname_job_s {
    char *hostname;
    /* outputs, written by the worker thread */
    bool found;                /* the resolver returned a hostent */
    size_t count;
    struct in_addr *addresses; /* copy of h_addr_list, moved to the caller after a successful wait */
} swow_gethostbyname_job_t;

static void swow_gethostbyname_job_run(cat_data_t *data)
{
    swow_gethostbyname_job_t *job = (swow_gethostbyname_job_t *) data;
    struct hostent *hp;
#ifdef HAVE_GETHOSTBYNAME_R
    struct hostent hostbuf;
    char *buffer = NULL;
    size_t buffer_size = 0;

    memset(&hostbuf, 0, sizeof(hostbuf));
    hp = swow_gethostname_re(job->hostname, &hostbuf, &buffer, &buffer_size);
#else
    hp = gethostbyname(job->hostname);
#endif
    if (hp != NULL) {
        size_t count = 0;
        job->found = true;
        for (;; count++) {
            /* On macos h_addr_list entries may be misaligned. */
            struct in_addr *h_addr_entry;
            memcpy(&h_addr_entry, &hp->h_addr_list[count], sizeof(struct in_addr *));
            if (!h_addr_entry) {
                break;
            }
        }
        if (count > 0) {
            job->addresses = (struct in_addr *) cat_sys_malloc(sizeof(*job->addresses) * count);
            for (size_t i = 0; i < count; i++) {
                struct in_addr *h_addr_entry;
                memcpy(&h_addr_entry, &hp->h_addr_list[i], sizeof(struct in_addr *));
                memcpy(&job->addresses[i], h_addr_entry, sizeof(struct in_addr));
            }
        }
        job->count = count;
    }
#ifdef HAVE_GETHOSTBYNAME_R
    if (buffer != NULL) {
        cat_sys_free(buffer);
    }
#endif
}

static void swow_gethostbyname_job_free(cat_data_t *data)
{
    swow_gethostbyname_job_t *job = (swow_gethostbyname_job_t *) data;

    cat_sys_free(job->hostname);
    if (job->addresses != NULL) {
        cat_sys_free(job->addresses);
    }
    cat_sys_free(job);
}

/* Behaves as ext/standard gethostbynamel(), but only suspends the current coroutine */
static PHP_FUNCTION(swow_gethostbynamel)
{
    char *hostname;
    size_t hostname_len;
    char addr4[INET_ADDRSTRLEN];

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_PATH(hostname, hostname_len)
    ZEND_PARSE_PARAMETERS_END();

    if (hostname_len > MAXFQDNLEN) {
        /* name too long, protect from CVE-2015-0235 */
        php_error_docref(NULL, E_WARNING, "Host name cannot be longer than %d characters", MAXFQDNLEN);
        RETURN_FALSE;
    }

    swow_gethostbyname_job_t *job = (swow_gethostbyname_job_t *) cat_sys_malloc(sizeof(*job));
    job->hostname = cat_sys_strdup(hostname);
    job->found = false;
    job->count = 0;
    job->addresses = NULL;
    if (!cat_work(CAT_WORK_KIND_SLOW_IO, swow_gethostbyname_job_run, swow_gethostbyname_job_free, job, cat_socket_get_global_dns_timeout())) {
        /* same as a failed lookup, which PHP reports by returning false only */
        RETURN_FALSE;
    }
    /* the job is freed once this coroutine yields again (a warning handler may do so), take the
     * addresses right now */
    if (!job->found) {
        RETURN_FALSE;
    }
    struct in_addr *addresses = job->addresses;
    size_t count = job->count;
    job->addresses = NULL;

    array_init(return_value);
    for (size_t i = 0; i < count; i++) {
        const char *ipaddr;
        if (!(ipaddr = inet_ntop(AF_INET, &addresses[i], addr4, INET_ADDRSTRLEN))) {
            /* unlikely regarding (too) long hostname and protocols but checking still */
            php_error_docref(NULL, E_WARNING, "Host name to ip failed %s", hostname);
            continue;
        } else {
            add_next_index_string(return_value, ipaddr);
        }
    }
    if (addresses != NULL) {
        cat_sys_free(addresses);
    }
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_swow_gethostbynamel, 0, 1, MAY_BE_ARRAY|MAY_BE_FALSE)
    ZEND_ARG_TYPE_INFO(0, hostname, IS_STRING, 0)
ZEND_END_ARG_INFO()

#endif /* SWOW_DNS_HAVE_GETHOSTBYNAMEL */
/* }}} */

#ifdef HAVE_IPV6
/* Behaves as ext/standard gethostbyaddr(), but runs getnameinfo() in the thread pool */
static PHP_FUNCTION(swow_gethostbyaddr)
{
    char *addr;
    size_t addr_len;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_PATH(addr, addr_len)
    ZEND_PARSE_PARAMETERS_END();

    struct sockaddr_in sa4;
    struct sockaddr_in6 sa6;
    const struct sockaddr *address;
    memset(&sa4, 0, sizeof(sa4));
    memset(&sa6, 0, sizeof(sa6));
    if (inet_pton(AF_INET6, addr, &sa6.sin6_addr)) {
        sa6.sin6_family = AF_INET6;
        address = (const struct sockaddr *) &sa6;
    } else if (inet_pton(AF_INET, addr, &sa4.sin_addr)) {
        sa4.sin_family = AF_INET;
        address = (const struct sockaddr *) &sa4;
    } else {
        php_error_docref(NULL, E_WARNING, "Address is not a valid IPv4 or IPv6 address");
        RETURN_FALSE;
    }

    char out[NI_MAXHOST];
    if (!cat_dns_getnameinfo(address, out, sizeof(out), NULL, 0, NI_NAMEREQD)) {
        /* same as PHP: an address without a usable PTR record resolves to itself */
        RETURN_STRINGL(addr, addr_len);
    }
    RETURN_STRING(out);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_swow_gethostbyaddr, 0, 1, MAY_BE_STRING|MAY_BE_FALSE)
    ZEND_ARG_TYPE_INFO(0, ip, IS_STRING, 0)
ZEND_END_ARG_INFO()
#endif

static const zend_function_entry swow_dns_functions[] = {
    PHP_FENTRY(gethostbyname, PHP_FN(swow_gethostbyname), arginfo_swow_gethostbyname, 0)
    PHP_FENTRY(gethostbyname2, PHP_FN(swow_gethostbyname2), arginfo_swow_gethostbyname2, 0)
#ifdef SWOW_DNS_HAVE_GETHOSTBYNAMEL
    PHP_FENTRY(gethostbynamel, PHP_FN(swow_gethostbynamel), arginfo_swow_gethostbynamel, 0)
#endif
#ifdef HAVE_IPV6
    PHP_FENTRY(gethostbyaddr, PHP_FN(swow_gethostbyaddr), arginfo_swow_gethostbyaddr, 0)
#endif
    PHP_FE_END
};

static bool has_sockets_extension = false;
static bool af_constants_checked = false;

#define SWOW_OBTAIN_CONSTANT_OWNERSHIP(name) swow_obtain_constant_ownership(ZEND_STRL(name), module_number)

static void swow_obtain_constant_ownership(const char *name, size_t name_length, int module_number)
{
    zend_constant *c = (zend_constant *) zend_hash_str_find_ptr(EG(zend_constants), name, name_length);
    ZEND_ASSERT(c != NULL);
    ZEND_CONSTANT_SET_FLAGS(c, ZEND_CONSTANT_FLAGS(c), module_number);
}

zend_result swow_dns_module_init(INIT_FUNC_ARGS)
{
    if (!swow_hook_internal_functions(swow_dns_functions)) {
        return FAILURE;
    }
#ifdef SWOW_DNS_HAVE_RECORD_FUNCTIONS
    /* replace handlers of existing functions only, PHP may not provide some of them */
    for (const zend_function_entry *fe = swow_dns_record_functions; fe->fname != NULL; fe++) {
        (void) swow_hook_internal_function_handler(fe->fname, strlen(fe->fname), fe->handler);
    }
#endif

    if (!zend_hash_str_exists(EG(zend_constants), ZEND_STRL("AF_UNSPEC"))) {
        REGISTER_LONG_CONSTANT("AF_UNSPEC", AF_UNSPEC, CONST_PERSISTENT);
    }
    if (!zend_hash_str_find_ptr(&module_registry, ZEND_STRL("sockets"))) {
        if (!zend_hash_str_exists(EG(zend_constants), ZEND_STRL("AF_INET"))) {
            REGISTER_LONG_CONSTANT("AF_INET", AF_INET, CONST_PERSISTENT);
        }
        if (!zend_hash_str_exists(EG(zend_constants), ZEND_STRL("AF_INET6"))) {
            REGISTER_LONG_CONSTANT("AF_INET6", AF_INET6, CONST_PERSISTENT);
        }
        // TODO: others?
    } else {
        has_sockets_extension = true;
    }

    return SUCCESS;
}

zend_result swow_dns_runtime_init(INIT_FUNC_ARGS)
{
    if (!af_constants_checked) {
        if (has_sockets_extension) {
            SWOW_OBTAIN_CONSTANT_OWNERSHIP("AF_INET");
            SWOW_OBTAIN_CONSTANT_OWNERSHIP("AF_INET6");
        }
        af_constants_checked = true;
    }

    return SUCCESS;
}
