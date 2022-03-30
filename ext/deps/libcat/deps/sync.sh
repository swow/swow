#!/bin/bash
__DIR__=$(cd "$(dirname "$0")" || exit 1; pwd); [ -z "${__DIR__}" ] && exit 1

BOOST_CONTEXT_VERSION="develop"
LIBUV_VERSION="develop"
LLHTTP_VERSION="v6.0.6"

sync(){ "${__DIR__}/../tools/dm.sh" "${__DIR__}/../" "$@"; }

# ====== boost-context ======

context_dir="${__DIR__}/context"

sync_boost_context()
{
  sync \
  "boostorg" \
  "context" \
  "https://github.com/boostorg/context.git" \
  "${BOOST_CONTEXT_VERSION}" \
  "context/src/asm" \
  "${context_dir}/asm"
}

if sync_boost_context  && cd "${context_dir}/asm"; then
  ALL_FILES=()
  while IFS='' read -r line; do ALL_FILES+=("$line"); done < <(ls ./*.asm ./*.S)

  perl -p -i -e 's;\b_make_fcontext\b;_cat_coroutine_context_make;g' "${ALL_FILES[@]}";
  perl -p -i -e 's;\bmake_fcontext\b;cat_coroutine_context_make;g' "${ALL_FILES[@]}";
  perl -p -i -e 's;\b_jump_fcontext\b;_cat_coroutine_context_jump;g' "${ALL_FILES[@]}";
  perl -p -i -e 's;\bjump_fcontext\b;cat_coroutine_context_jump;g' "${ALL_FILES[@]}";
  perl -p -i -e 's;\b_ontop_fcontext\b;_cat_coroutine_context_ontop;g' "${ALL_FILES[@]}";
  perl -p -i -e 's;\bontop_fcontext\b;cat_coroutine_context_ontop;g' "${ALL_FILES[@]}";

  perl -p -i -e 's;\bBOOST_CONTEXT_EXPORT\b;EXPORT;g' "${ALL_FILES[@]}";
  perl -p -i -e 's;\bBOOST_USE_TSX\b;CAT_USE_TSX;g' "${ALL_FILES[@]}";

  find . \( -name \*.cpp \) -print0 | xargs -0 rm -f

  wget -x -O "${context_dir}/LICENSE" \
  "https://raw.githubusercontent.com/boostorg/boost/master/LICENSE_1_0.txt"
fi

# ====== libuv ======

sync_libuv()
{
  sync \
  "libcat" \
  "libuv" \
  "https://github.com/libcat/libuv.git" \
  "${LIBUV_VERSION}" \
  "libuv" \
  "${__DIR__}/libuv"
}

if sync_libuv ; then
  mv "${__DIR__}/libuv" "${__DIR__}/_libuv" && \
  mkdir -p "${__DIR__}/libuv" && \
  cp -r "${__DIR__}/_libuv/include" "${__DIR__}/libuv/include" && \
  cp -r "${__DIR__}/_libuv/src" "${__DIR__}/libuv/src" && \
  cp "${__DIR__}/_libuv/LICENSE" "${__DIR__}/libuv/LICENSE" && \
  cp "${__DIR__}/_libuv/AUTHORS" "${__DIR__}/libuv/AUTHORS"
  rm -rf "${__DIR__}/_libuv"
fi

# ====== llhttp ======

sync_llhttp()
{
  sync \
  "nodejs" \
  "llhttp" \
  "https://github.com/nodejs/llhttp/archive/release/${LLHTTP_VERSION}.tar.gz" \
  "${LLHTTP_VERSION}" \
  "llhttp-release-${LLHTTP_VERSION}" \
  "${__DIR__}/llhttp"
}

if sync_llhttp ; then
  mv "${__DIR__}/llhttp" "${__DIR__}/_llhttp" && \
  mkdir -p "${__DIR__}/llhttp" && \
  cp -r "${__DIR__}/_llhttp/include" "${__DIR__}/llhttp/include" && \
  cp -r "${__DIR__}/_llhttp/src" "${__DIR__}/llhttp/src" && \
  cp -r "${__DIR__}/_llhttp/LICENSE-MIT" "${__DIR__}/llhttp/LICENSE-MIT" && \
  rm -rf "${__DIR__}/_llhttp"
fi

cd "${__DIR__}" && \
git add --ignore-errors -A
