/*
  +--------------------------------------------------------------------------+
  | libcat                                                                   |
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

#include "cat_dns.h"
#include "cat_coroutine.h"
#include "cat_event.h"
#include "cat_time.h"

typedef struct cat_getaddrinfo_context_s {
    union {
        cat_coroutine_t *coroutine;
        uv_req_t req;
        uv_getaddrinfo_t getaddrinfo;
    } request;
    int status;
    struct addrinfo *response;
} cat_getaddrinfo_context_t;

static void cat_dns_getaddrinfo_callback(uv_getaddrinfo_t* request, int status, struct addrinfo *response)
{
    cat_getaddrinfo_context_t *context = cat_container_of(request, cat_getaddrinfo_context_t, request.getaddrinfo);

    if (likely(context->request.coroutine != NULL)) {
        context->status = status;
        context->response = response;
        cat_coroutine_schedule(context->request.coroutine, DNS, "DNS resolver");
    } else if (response != NULL) {
        uv_freeaddrinfo(response);
    }

    cat_free(context);
}

CAT_API struct addrinfo *cat_dns_getaddrinfo(const char *hostname, const char *service, const struct addrinfo *hints)
{
    return cat_dns_getaddrinfo_ex(hostname, service, hints, cat_socket_get_global_dns_timeout());
}

CAT_API struct addrinfo *cat_dns_getaddrinfo_ex(const char *hostname, const char *service, const struct addrinfo *hints, cat_timeout_t timeout)
{
    cat_getaddrinfo_context_t *context = (cat_getaddrinfo_context_t *) cat_malloc(sizeof(*context));
    cat_bool_t ret;
    int error;

#if CAT_ALLOC_HANDLE_ERRORS
    if (unlikely(context == NULL)) {
        cat_update_last_error_of_syscall("Malloc for DNS getaddrinfo context failed");
        return NULL;
    }
#endif
    error = uv_getaddrinfo(&CAT_EVENT_G(loop), &context->request.getaddrinfo, cat_dns_getaddrinfo_callback, hostname, service, hints);
    if (error != 0) {
        cat_update_last_error_with_reason(error, "DNS getaddrinfo init failed");
        cat_free(context);
        return NULL;
    }
    context->status = CAT_ECANCELED;
    context->request.coroutine = CAT_COROUTINE_G(current);
    ret = cat_time_wait(timeout);
    context->request.coroutine = NULL;
    if (unlikely(!ret)) {
        cat_update_last_error_with_previous("DNS getaddrinfo wait failed");
        (void) uv_cancel(&context->request.req);
        return NULL;
    }
    if (unlikely(context->status != 0)) {
        if (context->status == CAT_ECANCELED) {
            cat_update_last_error(CAT_ECANCELED, "DNS getaddrinfo has been canceled");
            (void) uv_cancel(&context->request.req);
        } else {
            cat_update_last_error_with_reason(context->status, "DNS getaddrinfo failed");
        }
        return NULL;
    }

    return context->response;
}

CAT_API void cat_dns_freeaddrinfo(struct addrinfo *response)
{
    uv_freeaddrinfo(response);
}

CAT_API cat_bool_t cat_dns_get_ip(char *buffer, size_t buffer_size, const char *name, int af)
{
    return cat_dns_get_ip_ex(buffer, buffer_size, name, af, cat_socket_get_global_dns_timeout());
}

CAT_API cat_bool_t cat_dns_get_ip_ex(char *buffer, size_t buffer_size, const char *name, int af, cat_timeout_t timeout)
{
    struct addrinfo hints;
    struct addrinfo *response;
    int error;

    hints.ai_family = af;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = 0;
    response = cat_dns_getaddrinfo_ex(name, NULL, &hints, timeout);
    if (unlikely(response == NULL)) {
        cat_update_last_error_with_previous("DNS get ip failed");
        return cat_false;
    }
    switch (response->ai_family) {
        case AF_INET:
            error = uv_ip4_name((struct sockaddr_in *) response->ai_addr, buffer, buffer_size);
            break;
        case AF_INET6:
            error = uv_ip6_name((struct sockaddr_in6 *) response->ai_addr, buffer, buffer_size);
            break;
        default:
            error = CAT_EAFNOSUPPORT;
            break;
    }
    cat_dns_freeaddrinfo(response);
    if (unlikely(error != 0)) {
        cat_update_last_error_with_reason(error, "DNS convert address to IP name failed");
        return cat_false;
    }

    return cat_true;
}
