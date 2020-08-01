#!/bin/bash
__DIR__=$(cd "$(dirname "$0")" || exit 1; pwd); [ -z "${__DIR__}" ] && exit 1

ulimit -n 8192

export SERVER_HOST=127.0.0.1
export SERVER_PORT=9764
export SERVER_BACKLOG=8192
/usr/bin/env php -dextension=swow "${__DIR__}/../examples/http_server/echo.php" &
pid=$!

sleep 1
ab -c 1024 -n 1000000 -k "http://${SERVER_HOST}:${SERVER_PORT}/"

kill ${pid}
wait ${pid}
