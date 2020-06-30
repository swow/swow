#!/bin/bash
__DIR__=$(cd "$(dirname "$0")" || exit 1; pwd); [ -z "${__DIR__}" ] && exit 1

USE_AUTOTOOLS=1 "${__DIR__}"/deps/libcat/clean.sh
"${__DIR__}"/deps/libcat/tools/cleaner.sh "${__DIR__}"
