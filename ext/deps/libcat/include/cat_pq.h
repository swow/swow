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

#ifndef CAT_PQ_H
#define CAT_PQ_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"

#ifdef CAT_HAVE_PQ
#define CAT_PQ 1

#include <libpq-fe.h>
#include <libpq/libpq-fs.h>
// #include <libpq-int.h>

CAT_API cat_bool_t cat_pq_runtime_init(void);
CAT_API cat_bool_t cat_pq_runtime_close(void);

CAT_API PGconn *cat_pq_connectdb(const char *conninfo);
CAT_API PGresult *cat_pq_prepare(PGconn *conn, const char *stmt_name, const char *query, int n_params, const Oid *param_types);
CAT_API PGresult *cat_pq_exec_prepared(PGconn *conn, const char *stmt_name, int n_params, 
    const char *const *param_values, const int *param_lengths, const int *param_formats, int result_format);
CAT_API PGresult *cat_pq_exec(PGconn *conn, const char *query);
CAT_API PGresult *cat_pq_exec_params(PGconn *conn, const char *command, int n_params,
    const Oid *param_types, const char *const *param_values, const int *param_lengths, const int *param_formats, int result_format);

#endif /* CAT_HAVE_PQ */

#ifdef __cplusplus
}
#endif
#endif /* CAT_PQ_H */
