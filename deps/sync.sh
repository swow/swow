#!/bin/bash
__DIR__=$(cd "$(dirname "$0")" || exit 1; pwd); [ -z "${__DIR__}" ] && exit 1

sync(){ "${__DIR__}/libcat/tools/dm.sh" "${__DIR__}/../" "$@"; }

sync \
"libcat" \
"git@github.com:libcat/libcat.git" \
"libcat" \
"${__DIR__}/libcat"
