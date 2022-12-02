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

#include "swow.h"

/* PHP 8.1 compatibility {{{*/
#if PHP_VERSION_ID < 80100
SWOW_API zend_string* ZEND_FASTCALL zend_ulong_to_str(zend_ulong num)
{
    if (num <= 9) {
        return ZSTR_CHAR((zend_uchar) '0' + (zend_uchar) num);
    } else {
        char buf[MAX_LENGTH_OF_LONG + 1];
        char *res = zend_print_ulong_to_buf(buf + sizeof(buf) - 1, num);
        return zend_string_init(res, buf + sizeof(buf) - 1 - res, 0);
    }
}
#endif
/* }}} */

/* PHP 8.2 compatibility {{{*/

SWOW_API zend_op_array *swow_compile_string(zend_string *source_string, const char *filename)
{
    return swow_compile_string_ex(source_string, filename, ZEND_COMPILE_POSITION_AFTER_OPEN_TAG);
}

SWOW_API zend_op_array *swow_compile_string_ex(zend_string *source_string, const char *filename, zend_compile_position position)
{
#if PHP_VERSION_ID < 80200
    if (position != ZEND_COMPILE_POSITION_AFTER_OPEN_TAG) {
        zend_throw_error(NULL, "Compile position require PHP-8.2+");
        return NULL;
    }
    return zend_compile_string(source_string, filename);
#else
    return zend_compile_string(source_string, filename, position);
#endif
}
/* }}} */

/* class */

SWOW_API zend_class_entry *swow_register_internal_class(
    const char *name, zend_class_entry *parent_ce, const zend_function_entry methods[],
    zend_object_handlers *handlers, const zend_object_handlers *parent_handlers,
    const cat_bool_t cloneable,const cat_bool_t serializable,
    const swow_object_create_t create_object, const swow_object_free_t free_object, const int offset
)
{
    zend_class_entry *ce, _ce;
    cat_bool_t is_final; /* ignore final flag */

    INIT_CLASS_ENTRY_EX(_ce, name, strlen(name), methods);
    if ((is_final = (parent_ce && !!(parent_ce->ce_flags & ZEND_ACC_FINAL)))) {
        parent_ce->ce_flags ^= ZEND_ACC_FINAL;
    }
    ce = zend_register_internal_class_ex(&_ce, parent_ce);
    if (is_final) {
        ce->ce_flags |= ZEND_ACC_FINAL;
        parent_ce->ce_flags |= ZEND_ACC_FINAL;
    }
    if (!serializable) {
#ifdef ZEND_ACC_NOT_SERIALIZABLE /* PHP_VERSION_ID >= 80100 */
        ce->ce_flags |= ZEND_ACC_NOT_SERIALIZABLE;
#else
        ce->serialize = zend_class_serialize_deny;
        ce->unserialize = zend_class_unserialize_deny;
#endif
    }
    if (create_object != NULL) {
        ce->create_object = create_object;
    } else if (parent_ce != NULL) {
        ce->create_object = parent_ce->create_object;
    }
    if (handlers) {
        ZEND_ASSERT((
            parent_handlers != NULL ||
            !cloneable ||
            free_object != NULL ||
            offset != 0
        ) &&  "Unused handlers");
        if (parent_handlers == NULL) {
            parent_handlers = zend_get_std_object_handlers();
        }
        memcpy(handlers, parent_handlers, sizeof(*parent_handlers));
        if (!cloneable) {
            handlers->clone_obj = NULL;
        }
        if (free_object != NULL) {
            handlers->free_obj = free_object;
        }
        if (offset != 0) {
            handlers->offset = offset;
            if (cloneable) {
                handlers->clone_obj = swow_custom_object_clone;
            }
        }
    } else {
        ZEND_ASSERT(parent_handlers == NULL);
        ZEND_ASSERT(create_object == swow_create_object_deny || cloneable);
        ZEND_ASSERT(free_object == NULL);
        ZEND_ASSERT(offset == 0);
    }

    return ce;
}

SWOW_API zend_class_entry *swow_register_internal_interface(
    const char *name,
    const zend_function_entry methods[],
    swow_interface_gets_implemented_t interface_gets_implemented
)
{
    zend_class_entry *ce, _ce;

    INIT_CLASS_ENTRY_EX(_ce, name, strlen(name), methods);
    ce = zend_register_internal_interface(&_ce);
    ce->interface_gets_implemented = interface_gets_implemented;

    return ce;
}

SWOW_API zend_object *swow_create_object_deny(zend_class_entry *ce)
{
    zend_object *object;

    object = zend_objects_new(ce);
    /* Initialize default properties */
    if (EXPECTED(ce->default_properties_count != 0)) {
        zval *p = object->properties_table;
        zval *end = p + ce->default_properties_count;
        do {
            ZVAL_UNDEF(p);
            p++;
        } while (p != end);
    }
    zend_throw_error(NULL, "The object of %s can not be created for security reasons", ZSTR_VAL(ce->name));

    return object;
}

SWOW_API zend_object *swow_custom_object_clone(zend_object *object)
{
    zend_object *old_object = object, *new_object;
    zend_class_entry *ce = old_object->ce;
    int offset = old_object->handlers->offset;

    new_object = swow_object_create(ce);
    memcpy((((char *) new_object) - offset), (((char *) old_object) - offset), offset);

    zend_objects_clone_members(new_object, old_object);

    return new_object;
}

/* callable */

SWOW_API zend_bool swow_fcall_storage_is_available(const swow_fcall_storage_t *fcall)
{
    return Z_TYPE(fcall->z_callable) != IS_UNDEF;
}

SWOW_API zend_bool swow_fcall_storage_create(swow_fcall_storage_t *fcall, zval *z_callable)
{
    if (Z_TYPE_P(z_callable) == IS_PTR) {
        *fcall = *((swow_fcall_storage_t *) Z_PTR_P(z_callable));
        Z_TRY_ADDREF(fcall->z_callable);
    } else {
        char *error;
        if (!zend_is_callable_ex(z_callable, NULL, 0, NULL, &fcall->fcc, &error)) {
            cat_update_last_error(CAT_EMISUSE, "The argument passed in is not callable (%s)", error);
            efree(error);
            return false;
        }
        ZEND_ASSERT(!error);
        ZVAL_COPY(&fcall->z_callable, z_callable);
    }

    return true;
}

SWOW_API void swow_fcall_storage_release(swow_fcall_storage_t *fcall)
{
    zval_ptr_dtor(&fcall->z_callable);
    ZVAL_UNDEF(&fcall->z_callable);
}

/* function caller */

SWOW_API zend_result swow_call_function_anyway(zend_fcall_info *fci, zend_fcall_info_cache *fci_cache)
{
    zend_exception_save();
    zend_result ret = zend_call_function(fci, fci_cache);
    zend_exception_restore();
    return ret;
}

/* var_dump */

SWOW_API void swow_var_dump_string(zend_string *string)
{
    swow_var_dump_string_ex(string, 0);
}

SWOW_API void swow_var_dump_string_ex(zend_string *string, int level)
{
    zval z_tmp;
    ZVAL_STR(&z_tmp, string);
    php_var_dump(&z_tmp, level);
}

SWOW_API void swow_var_dump_array(zend_array *array)
{
    swow_var_dump_array_ex(array, 0);
}

SWOW_API void swow_var_dump_array_ex(zend_array *array, int level)
{
    zval z_tmp;
    ZVAL_ARR(&z_tmp, array);
    php_var_dump(&z_tmp, level);
}

SWOW_API void swow_var_dump_object(zend_object *object)
{
    swow_var_dump_object_ex(object, 0);
}

SWOW_API void swow_var_dump_object_ex(zend_object *object, int level)
{
    zval z_tmp;
    ZVAL_OBJ(&z_tmp, object);
    php_var_dump(&z_tmp, level);
}

/* output globals */

#include "SAPI.h"

SWOW_API void swow_output_globals_init(void)
{
    SWOW_OUTPUT_GLOBALS_MODIFY_START() {
        php_output_activate();
    } SWOW_OUTPUT_GLOBALS_MODIFY_END();
}

SWOW_API void swow_output_globals_shutdown(void)
{
    SWOW_OUTPUT_GLOBALS_MODIFY_START() {
        zend_bool no_headers = SG(request_info).no_headers;

        /* Do not send headers by SAPI */
        SG(request_info).no_headers = 1;
#ifdef SWOW_OUTPUT_GLOBALS_START_FILENAME
        if (output_start_filename != NULL) {
            zend_string_addref(output_start_filename);
        }
#endif

        if (OG(active)) {
            php_output_end_all();
        }
        php_output_deactivate();
        php_output_activate();

        SG(request_info).no_headers = no_headers;
    } SWOW_OUTPUT_GLOBALS_MODIFY_END();
}

/* file */

SWOW_API SWOW_MAY_THROW zend_string *swow_file_get_contents(zend_string *filename)
{
    zend_string *contents = NULL;

    SWOW_THROW_ON_ERROR_START() {
        php_stream *stream = php_stream_open_wrapper_ex(ZSTR_VAL(filename), "rb", REPORT_ERRORS, NULL, NULL);
        if (stream == NULL) {
            break;
        }
        contents = php_stream_copy_to_mem(stream, -1, 0);
        php_stream_close(stream);
    } SWOW_THROW_ON_ERROR_END();

    return contents;
}

/* wrapper init/shutdown */

void swow_wrapper_init(void)
{
    /* reserved */
}

void swow_wrapper_shutdown(void)
{
    /* reserved */
}
