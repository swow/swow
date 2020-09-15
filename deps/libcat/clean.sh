#!/bin/bash
__DIR__=$(cd "$(dirname "$0")" || exit 1; pwd); [ -z "${__DIR__}" ] && exit 1

"${__DIR__}"/tools/cleaner.sh "${__DIR__}"
