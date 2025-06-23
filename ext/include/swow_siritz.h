

#ifndef _SWOW_SIRITZ_H
#define _SWOW_SIRITZ_H

#include "swow.h"

typedef struct swow_siritz_s {
    smart_str callable;
    smart_str args;
    uv_thread_t thread;
    zend_object std;
} swow_siritz_t;

typedef struct swow_siritz_run_s {
    smart_str callable;
    smart_str args;
    void *server_context;
} swow_siritz_run_t;

static zend_always_inline swow_siritz_t *swow_siritz_get_from_object(zend_object *object)
{
    return cat_container_of(object, swow_siritz_t, std);
}

zend_result swow_siritz_module_init(INIT_FUNC_ARGS);
zend_result swow_siritz_module_shutdown(INIT_FUNC_ARGS);

#endif // _SWOW_SIRITZ_H