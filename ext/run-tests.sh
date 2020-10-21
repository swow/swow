#!/bin/bash
__DIR__=$(cd "$(dirname "$0")" || exit 1; pwd); [ -z "${__DIR__}" ] && exit 1

/usr/bin/env php -n ${__DIR__}/tools/c-tests/run-tests.php -P -d extension=swow $@
