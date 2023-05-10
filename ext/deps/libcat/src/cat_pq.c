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
  | Author: Codinghuang <codinghuang@qq.com>                                 |
  +--------------------------------------------------------------------------+
 */

#include "cat_pq.h"

#ifdef CAT_PQ

#include "cat_poll.h"

CAT_API cat_bool_t cat_pq_runtime_init(void)
{
    return cat_true;
}

CAT_API cat_bool_t cat_pq_runtime_close(void)
{
    return cat_true;
}

static int cat_pq_flush(PGconn *conn)
{
    int flush_ret = -1;

    do {
        cat_ret_t poll_ret = cat_poll_one(PQsocket(conn), POLLOUT, NULL, -1);
        if (unlikely(poll_ret == CAT_RET_ERROR)) {
            return -1;
        }
        CAT_LOG_DEBUG(PQ, "PQflush(conn=%p)", conn);
        flush_ret = PQflush(conn);
    } while (flush_ret == 1);

    return flush_ret;
}

static PGresult *cat_pq_get_result(PGconn *conn)
{
    PGresult *result, *last_result = NULL;
    cat_ret_t poll_ret = cat_poll_one(PQsocket(conn), POLLIN, NULL, -1);
    if (unlikely(poll_ret == CAT_RET_ERROR)) {
        return NULL;
    }

    CAT_LOG_DEBUG(PQ, "PQgetResult(conn=%p)", conn);
    while ((result = PQgetResult(conn))) {
        PQclear(last_result);
        last_result = result;
    }

    return last_result;
}

static cat_bool_t cat_pq_poll_connection(PGconn *conn)
{
    int r;

    while ((r = PQconnectPoll(conn)) && r != PGRES_POLLING_OK) {
        cat_pollfd_events_t events = POLLNONE;

        switch(r) {
        case PGRES_POLLING_READING:
            events |= POLLIN;
            break;
        case PGRES_POLLING_WRITING:
            events |= POLLOUT;
            break;
        case PGRES_POLLING_FAILED:
            return cat_false;
        default:
            return cat_false;
        }

        cat_ret_t ret = cat_poll_one(PQsocket(conn), events, NULL, -1);
        if (unlikely(ret == CAT_RET_ERROR)) {
            return cat_false;
        }
    }

    return cat_false;
}

CAT_API PGconn *cat_pq_connectdb(const char *conninfo)
{
    int fd;
    PGconn *conn = PQconnectStart(conninfo);
    if(conn == NULL) {
        return NULL;
    }

    fd = PQsocket(conn);
    if (unlikely(fd < 0)) {
        return conn;
    }

    PQsetnonblocking(conn, 1);

    cat_pq_poll_connection(conn);

    return conn;
}

CAT_API PGresult *cat_pq_prepare(PGconn *conn, const char *stmt_name, const char *query, int n_params, const Oid *param_types)
{
    CAT_LOG_DEBUG(PQ, "PQsendPrepare(conn=%p, stmt_name='%s')", conn, stmt_name);
    int ret = PQsendPrepare(conn, stmt_name, query, n_params, param_types);
    if (ret == 0) {
        return NULL;
    }

    if (cat_pq_flush(conn) == -1) {
        return NULL;
    }

    return cat_pq_get_result(conn);
}

CAT_API PGresult *cat_pq_exec_prepared(PGconn *conn, const char *stmt_name, int n_params,
    const char *const *param_values, const int *param_lengths, const int *param_formats, int result_format)
{
    CAT_LOG_DEBUG(PQ, "PQsendQueryPrepared(conn=%p, stmt_name='%s')", conn, stmt_name);
    int ret = PQsendQueryPrepared(conn, stmt_name, n_params, param_values, param_lengths, param_formats, result_format);
    if (ret == 0) {
        return NULL;
    }

    if (cat_pq_flush(conn) == -1) {
        return NULL;
    }

    return cat_pq_get_result(conn);
}

CAT_API PGresult *cat_pq_exec(PGconn *conn, const char *query)
{
    CAT_LOG_DEBUG(PQ, "PQsendQuery(conn=%p, query='%s')", conn, query);
    int ret = PQsendQuery(conn, query);
    if (ret == 0) {
        return NULL;
    }

    if (cat_pq_flush(conn) == -1) {
        return NULL;
    }

    return cat_pq_get_result(conn);
}

CAT_API PGresult *cat_pq_exec_params(PGconn *conn, const char *command, int n_params,
    const Oid *param_types, const char *const *param_values, const int *param_lengths, const int *param_formats, int result_format)
{
    CAT_LOG_DEBUG(PQ, "PQsendQueryParams(conn=%p, command='%s')", conn, command);
    int ret = PQsendQueryParams(conn, command, n_params, param_types, param_values, param_lengths, param_formats, result_format);
    if (ret == 0) {
        return NULL;
    }

    if (cat_pq_flush(conn) == -1) {
        return NULL;
    }

    return cat_pq_get_result(conn);
}

#endif /* CAT_PQ */
