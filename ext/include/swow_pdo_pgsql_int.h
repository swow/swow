/*
  +----------------------------------------------------------------------+
  | Copyright (c) The PHP Group                                          |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | https://www.php.net/license/3_01.txt                                 |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Edin Kadribasic <edink@emini.dk>                            |
  |          Ilia Alshanestsky <ilia@prohost.org>                        |
  |          Wez Furlong <wez@php.net>                                   |
  +----------------------------------------------------------------------+
*/

#ifndef PHP_PDO_PGSQL_INT_H
#define PHP_PDO_PGSQL_INT_H

// from ext/pdo_pgsql/php_pdo_pgsql_int.h
// php/php-src@50b3a0d011127b69e8432c37f98c87725981962f

#include <libpq-fe.h>
#include <libpq/libpq-fs.h>
#include <php.h>

#include "swow.h"
#include "swow_wrapper.h"

#define PHP_PDO_PGSQL_CONNECTION_FAILURE_SQLSTATE "08006"

typedef struct {
    const char *file;
    int line;
    unsigned int errcode;
    char *errmsg;
} pdo_pgsql_error_info;

typedef struct pdo_pgsql_stmt pdo_pgsql_stmt;

/* stuff we use in a pgsql database handle */
typedef struct {
    PGconn        *server;
    unsigned     attached:1;
    unsigned     _reserved:31;
    pdo_pgsql_error_info    einfo;
    Oid         pgoid;
    unsigned int    stmt_counter;
    bool        emulate_prepares;
#if PHP_VERSION_ID < 80500
    bool        disable_native_prepares;
#endif // PHP_VERSION_ID < 80500
    bool        disable_prepares;
    HashTable       *lob_streams;
    swow_fcall_info_cache *notice_callback;
    bool        default_fetching_laziness;
    pdo_pgsql_stmt  *running_stmt;
} pdo_pgsql_db_handle;

typedef struct {
// diff since php/php-src@caa710037e663fd78f67533b29611183090068b2
#if PHP_VERSION_ID < 80100
    char         *def;
    zend_long    intval;
    Oid          pgsql_type;
    zend_bool    boolval;
#else
    Oid          pgsql_type;
#endif
} pdo_pgsql_column;

struct pdo_pgsql_stmt {
    pdo_pgsql_db_handle     *H;
    PGresult                *result;
    pdo_pgsql_column        *cols;
    char *cursor_name;
    char *stmt_name;
// diff since php/php-src@2d51c203f09551323ed595514e03ab206fd93129
#if PHP_VERSION_ID < 80100
    char *query;
#else
    zend_string *query;
#endif // PHP_VERSION_ID
    char **param_values;
    int *param_lengths;
    int *param_formats;
    Oid *param_types;
    int                     current_row;
    bool is_prepared;
    bool is_unbuffered;
    bool is_running_unbuffered;
};

typedef struct {
    Oid     oid;
} pdo_pgsql_bound_param;

extern const pdo_driver_t pdo_pgsql_driver;

// diff since php/php-src@715b9aaa09e1ad76a94f32b17da7927592fdae0a
#if PHP_VERSION_ID >= 80400
extern int swow_pdo_pgsql_scanner(pdo_scanner_t *s);
#endif // PHP_VERSION_ID

extern int _swow_pdo_pgsql_error(pdo_dbh_t *dbh, pdo_stmt_t *stmt, int errcode, const char *sqlstate, const char *msg, const char *file, int line);
#define pdo_pgsql_error(d,e,z)    _swow_pdo_pgsql_error(d, NULL, e, z, NULL, __FILE__, __LINE__)
#define pdo_pgsql_error_msg(d,e,m)    _swow_pdo_pgsql_error(d, NULL, e, NULL, m, __FILE__, __LINE__)
#define pdo_pgsql_error_stmt(s,e,z)    _swow_pdo_pgsql_error(s->dbh, s, e, z, NULL, __FILE__, __LINE__)
#define pdo_pgsql_error_stmt_msg(stmt, e, sqlstate, msg) \
    _swow_pdo_pgsql_error(stmt->dbh, stmt, e, sqlstate, msg, __FILE__, __LINE__)

extern const struct pdo_stmt_methods swow_pgsql_stmt_methods;

#define pdo_pgsql_sqlstate(r) PQresultErrorField(r, PG_DIAG_SQLSTATE)

enum {
    PDO_PGSQL_ATTR_DISABLE_PREPARES = PDO_ATTR_DRIVER_SPECIFIC,
    PDO_PGSQL_ATTR_RESULT_MEMORY_SIZE,
};

struct pdo_pgsql_lob_self {
    zval dbh;
    PGconn *conn;
    int lfd;
    Oid oid;
};

enum pdo_pgsql_specific_constants {
    PGSQL_TRANSACTION_IDLE = PQTRANS_IDLE,
    PGSQL_TRANSACTION_ACTIVE = PQTRANS_ACTIVE,
    PGSQL_TRANSACTION_INTRANS = PQTRANS_INTRANS,
    PGSQL_TRANSACTION_INERROR = PQTRANS_INERROR,
    PGSQL_TRANSACTION_UNKNOWN = PQTRANS_UNKNOWN
};

php_stream *swow_pdo_pgsql_create_lob_stream(zend_object *dbh, int lfd, Oid oid);
extern const php_stream_ops swow_pdo_pgsql_lob_stream_ops;

void swow_pdo_libpq_version(char *buf, size_t len);
void swow_pdo_pgsql_close_lob_streams(pdo_dbh_t *dbh);

void swow_pgsqlCopyFromArray_internal(INTERNAL_FUNCTION_PARAMETERS);
void swow_pgsqlCopyFromFile_internal(INTERNAL_FUNCTION_PARAMETERS);
void swow_pgsqlCopyToArray_internal(INTERNAL_FUNCTION_PARAMETERS);
void swow_pgsqlCopyToFile_internal(INTERNAL_FUNCTION_PARAMETERS);
void swow_pgsqlLOBCreate_internal(INTERNAL_FUNCTION_PARAMETERS);
void swow_pgsqlLOBOpen_internal(INTERNAL_FUNCTION_PARAMETERS);
void swow_pgsqlLOBUnlink_internal(INTERNAL_FUNCTION_PARAMETERS);
void swow_pgsqlGetNotify_internal(INTERNAL_FUNCTION_PARAMETERS);
void swow_pgsqlGetPid_internal(INTERNAL_FUNCTION_PARAMETERS);

// end of ext/pdo_pgsql/php_pdo_pgsql_int.h

// wrapper for pq functions
extern PGresult *swow_PQclosePrepared(PGconn *conn, const char *stmtName);
extern size_t (*swow_PQresultMemorySize)(const PGresult *res);

// compatibility

#if PHP_VERSION_ID < 80100
bool pdo_get_long_param(zend_long *lval, const zval *value);
bool pdo_get_bool_param(bool *bval, const zval *value);
#endif // PHP_VERSION_ID < 80100

#if PHP_VERSION_ID < 80500
bool php_pdo_stmt_valid_db_obj_handle(const pdo_stmt_t *stmt);
#endif // PHP_VERSION_ID < 80500

#endif /* PHP_PDO_PGSQL_INT_H */
