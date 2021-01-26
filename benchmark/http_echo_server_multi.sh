#!/bin/bash
__DIR__=$(cd "$(dirname "$0")" || exit 1; pwd); [ -z "${__DIR__}" ] && exit 1

# shellcheck disable=SC2039
ulimit -n 10240

export SERVER_HOST=127.0.0.1
export SERVER_PORT=9764
export SERVER_BACKLOG=8192
export SERVER_MULTI=1

i=0
p=7
while [ ${i} -le ${p} ]; do
  /usr/bin/env php -dextension=swow -dmemory_limit=1G "${__DIR__}/../examples/http_server/echo.php" &
  processes[i]=$!;
  i=$((i+1));
done

sleep 1
ab -c 8192 -n 1000000 -k "http://${SERVER_HOST}:${SERVER_PORT}/"

i=0
while [ ${i} -le ${p} ]; do
  pid=${processes[i]}
  kill ${pid}
  wait ${pid}
  i=$((i+1));
done
