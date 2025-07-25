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

#define PHP_PDO_PGSQL_CONNECTION_FAILURE_SQLSTATE "08006"

typedef struct {
    const char *file;
    int line;
    unsigned int errcode;
    char *errmsg;
} pdo_pgsql_error_info;

/* stuff we use in a pgsql database handle */
typedef struct {
    PGconn        *server;
    unsigned     attached:1;
    unsigned     _reserved:31;
    pdo_pgsql_error_info    einfo;
    Oid         pgoid;
    unsigned int    stmt_counter;
    /* The following two variables have the same purpose. Unfortunately we need
       to keep track of two different attributes having the same effect. */
// diff since php/php-src@3e01f5afb1b52fe26a956190296de0192eedeec1
#if PHP_VERSION_ID <= 80100
    zend_bool        emulate_prepares;
    zend_bool        disable_native_prepares; /* deprecated since 5.6 */
    zend_bool        disable_prepares;
#else
    bool        emulate_prepares;
    bool        disable_native_prepares; /* deprecated since 5.6 */
    bool        disable_prepares;
#endif // PHP_VERSION_ID
    HashTable       *lob_streams;
// diff since php/php-src@c265b9085ae9b20bb37e0c1a052a1716827a8004
#if PHP_VERSION_ID >= 80400
    zend_fcall_info_cache *notice_callback;
#endif // PHP_VERSION_ID
} pdo_pgsql_db_handle;

// diff since php/php-src@caa710037e663fd78f67533b29611183090068b2
#if PHP_VERSION_ID < 80100
typedef struct {
    char         *def;
    zend_long    intval;
    Oid          pgsql_type;
    zend_bool    boolval;
} pdo_pgsql_column;
#else
typedef struct {
    Oid          pgsql_type;
} pdo_pgsql_column;
#endif
typedef struct {
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
// diff since php/php-src@3e01f5afb1b52fe26a956190296de0192eedeec1
#if PHP_VERSION_ID <= 80100
    zend_bool is_prepared;
#else
    bool is_prepared;
#endif // PHP_VERSION_ID
} pdo_pgsql_stmt;

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

php_stream *swow_pdo_pgsql_create_lob_stream(zval *pdh, int lfd, Oid oid);
extern const php_stream_ops swow_pdo_pgsql_lob_stream_ops;

// diff since php/php-src@a9259c04969eefabf4c66a8843a66d0bee1c56c0
#if PHP_VERSION_ID >= 80400
void swow_pdo_pgsql_cleanup_notice_callback(pdo_pgsql_db_handle *H);
#endif //PHP_VERSION_ID

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
extern PGresult *(*swow_PQclosePrepared)(PGconn *conn, const char *stmtName);
extern size_t (*swow_PQresultMemorySize)(const PGresult *res);

#endif /* PHP_PDO_PGSQL_INT_H */
