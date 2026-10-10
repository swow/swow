/* MongoDB integration uses the driver's public ABI, never private objects. */
#ifndef SWOW_MONGODB_H
#define SWOW_MONGODB_H
#include "swow.h"
#ifdef SWOW_HAVE_MONGODB
zend_result swow_mongodb_module_init(INIT_FUNC_ARGS);
zend_result swow_mongodb_runtime_init(INIT_FUNC_ARGS);
zend_result swow_mongodb_runtime_shutdown(SHUTDOWN_FUNC_ARGS);
#endif
#endif
