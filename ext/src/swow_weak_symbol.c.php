<?php

declare(strict_types=1);

?>
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
  | Author: Yun Dou <dixyes@gmail.com>                                       |
  +--------------------------------------------------------------------------+
 */
// weak symbol reference by dl

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __GNUC__
#include <dlfcn.h>
#endif

#include "config.h"
<?php

function weak(string $name, string $ret, string $argsSign, string $argsPassthruSign) {
    $return = $ret === "void" ? "$name($argsPassthruSign);\n    return;" : "return $name($argsPassthruSign);";
    echo <<<C


// weak function pointer for $name
__attribute__((weak, alias("swow_{$name}_redirect"))) extern $ret $name($argsSign);
// resolved function holder
$ret (*swow_{$name}_resolved)($argsSign);
// resolver for $name
$ret swow_{$name}_resolver($argsSign) {
    swow_{$name}_resolved = ($ret (*)($argsSign))dlsym(NULL, "$name");

    if (swow_{$name}_resolved == NULL) {
        fprintf(stderr, "failed resolve $name: %s\\n", dlerror());
        abort();
    } 

    $return
}
$ret (*swow_{$name}_resolved)($argsSign) = swow_{$name}_resolver;
$ret swow_{$name}_redirect($argsSign) {
    return swow_{$name}_resolved($argsPassthruSign);
}
C;
}

function weaks(string $signs) {
    // dammy signature parser
    foreach (explode("\n", $signs) as $line) {
        preg_match("/^(?P<ret_name>[^(]+)\((?P<args>.+)\);$/", $line, $match);
        if (!$match) {
            continue;
        }

        $ret_name = $match['ret_name'];
        $argsSign = $match['args'];
        preg_match("/^(?P<type>.*[*\s]+)(?P<name>.+)$/", $ret_name, $match);
        $ret = trim($match['type']) ?: "void";
        $name = trim($match['name']);

        if ($argsSign == "void") {
            $argsPassthruSign = "";
        } else {
            $argsPassthru = [];
            foreach (explode(",", $argsSign) as $argSign) {
                preg_match("/^(?P<type>.*[*\s]+)(?P<name>.+)$/", $argSign, $match);
                // $argType = trim($match['$type']);
                $argName = trim($match['name']);
                $argsPassthru[] = $argName;
            }

            $argsPassthruSign = implode(", ", $argsPassthru);
        }

        weak($name, $ret, $argsSign, $argsPassthruSign);
    }
}
?>

#ifdef CAT_HAVE_PQ
<?php
weaks(<<<C
int  PQbackendPID(const void *conn);
void PQclear(void *res);
char *PQcmdTuples(void *res);
int PQconnectPoll(void *conn);
void *PQconnectStart(const char *conninfo);
int  PQconsumeInput(void *conn);
char *PQerrorMessage(const void *conn);
unsigned char *PQescapeByteaConn(void *conn, const unsigned char *from, size_t from_length, size_t *to_length);
size_t PQescapeStringConn(void *conn, char *to, const char *from, size_t length, int *error);
void PQfinish(void *conn);
int  PQflush(void *conn);
int  PQfmod(const void *res, int field_num);
char *PQfname(const void *res, int field_num);
void PQfreemem(void *ptr);
int  PQfsize(const void *res, int field_num);
unsigned int  PQftable(const void *res, int field_num);
unsigned int  PQftype(const void *res, int field_num);
int  PQgetCopyData(void *conn, char **buffer, int async);
int  PQgetisnull(const void *res, int tup_num, int field_num);
int  PQgetlength(const void *res, int tup_num, int field_num);
void *PQgetResult(void *conn);
char *PQgetvalue(const void *res, int tup_num, int field_num);
int  PQlibVersion(void);
int  PQnfields(const void *res);
int  PQntuples(const void *res);
void *PQnotifies(void *conn);
unsigned int  PQoidValue(const void *res);
const char *PQparameterStatus(const void *conn, const char *paramName);
int  PQprotocolVersion(const void *conn);
int  PQputCopyData(void *conn, const char *buffer, int nbytes);
int  PQputCopyEnd(void *conn, const char *errormsg);
int  PQresetStart(void *conn);
char *PQresultErrorField(const void *res, int fieldcode);
int PQresultStatus(const void *res);
int  PQsendPrepare(void *conn, const char *stmtName, const char *query, int nParams, const unsigned int *paramTypes);
int  PQsendQuery(void *conn, const char *query);
int  PQsendQueryParams(void *conn, const char *command, int nParams,  const unsigned int *paramTypes, const char *const *paramValues, const int *paramLengths, const int *paramFormats, int resultFormat);
int  PQsendQueryPrepared(void *conn, const char *stmtName, int nParams, const char *const *paramValues, const int *paramLengths, const int *paramFormats, int resultFormat);
int  PQsetnonblocking(void *conn, int arg);
void *PQsetNoticeProcessor(void *conn, void *proc, void *arg);
int  PQsocket(const void *conn);
int  PQstatus(const void *conn);
int PQtransactionStatus(const void *conn);
unsigned char *PQunescapeBytea(const unsigned char *strtext, size_t *retbuflen);

bool pdo_get_bool_param(bool *bval, void *value);
void pdo_handle_error(void *dbh, void *stmt);
int pdo_parse_params(void *stmt, void *inquery, void *outquery);
void pdo_throw_exception(unsigned int driver_errcode, char *driver_errmsg, void *pdo_error);
int php_pdo_register_driver(const void *driver);
void php_pdo_unregister_driver(const void *driver);
C);
echo "\n";
?>
#endif
