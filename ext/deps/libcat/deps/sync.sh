#!/bin/bash
__DIR__=$(cd "$(dirname "$0")" || exit 1; pwd); [ -z "${__DIR__}" ] && exit 1

BOOST_CONTEXT_VERSION="develop"
LIBUV_VERSION="develop"
LLHTTP_VERSION="v8.1.0"
MULTIPART_PARSER_C_VERSION="master"

sync(){ "${__DIR__}/../tools/dm.sh" "${__DIR__}/../" "$@"; }

with_all=1
with_boost_context=0
with_libuv=0
with_llhttp=0
with_multipart_parser_c=0

for arg in "$@"; do
  case $arg in
    --specified=*)
      with_all=0
      IFS=',' read -ra values <<< "${arg#*=}"
      for val in "${values[@]}"; do
        case $val in
          boost-context)
            with_boost_context=1
            ;;
          libuv)
            with_libuv=1
            ;;
          llhttp)
            with_llhttp=1
            ;;
          multipart-parser-c)
            with_multipart_parser_c=1
            ;;
          *)
            echo "unknown specified: $val"
            exit 1
            ;;
        esac
      done
      ;;
  esac
done

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

if [[ $with_all -eq 1 || $with_boost_context -eq 1 ]]; then
  echo "sync boost-context..."
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
    echo "sync boost-context done"
  fi
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

if [[ $with_all -eq 1 || $with_libuv -eq 1 ]] ; then
  echo "sync libuv..."
  if sync_libuv ; then
    mv "${__DIR__}/libuv" "${__DIR__}/_libuv" && \
    mkdir -p "${__DIR__}/libuv" && \
    cp -r "${__DIR__}/_libuv/include" "${__DIR__}/libuv/include" && \
    cp -r "${__DIR__}/_libuv/src" "${__DIR__}/libuv/src" && \
    cp "${__DIR__}/_libuv/LICENSE" "${__DIR__}/libuv/LICENSE" && \
    cp "${__DIR__}/_libuv/AUTHORS" "${__DIR__}/libuv/AUTHORS"
    rm -rf "${__DIR__}/_libuv"
    echo "sync libuv done"
  fi
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

if [[ $with_all -eq 1 || $with_llhttp -eq 1 ]] ; then
  echo "sync llhttp..."
  if sync_llhttp ; then
    mv "${__DIR__}/llhttp" "${__DIR__}/_llhttp" && \
    mkdir -p "${__DIR__}/llhttp" && \
    cp -r "${__DIR__}/_llhttp/include" "${__DIR__}/llhttp/include" && \
    cp -r "${__DIR__}/_llhttp/src" "${__DIR__}/llhttp/src" && \
    cp -r "${__DIR__}/_llhttp/LICENSE-MIT" "${__DIR__}/llhttp/LICENSE-MIT" && \
    rm -rf "${__DIR__}/_llhttp"
    echo "sync llhttp done"
  fi
fi

# ====== multipart-parser-c ======

sync_multipart_parser_c()
{
  sync \
  "libcat" \
  "multipart-parser-c" \
  "https://github.com/libcat/multipart-parser-c.git" \
  "${MULTIPART_PARSER_C_VERSION}" \
  "multipart-parser-c" \
  "${__DIR__}/multipart-parser-c"
}

if [[ $with_all -eq 1 || $with_multipart_parser_c -eq 1 ]] ; then
  echo "sync multipart-parser-c..."
  if sync_multipart_parser_c ; then
    mv "${__DIR__}/multipart-parser-c" "${__DIR__}/_multipart-parser-c" && \
    mkdir -p "${__DIR__}/multipart-parser-c" && \
    cp "${__DIR__}/_multipart-parser-c/multipart_parser.c" \
       "${__DIR__}/multipart-parser-c/multipart_parser.c" && \
    cp "${__DIR__}/_multipart-parser-c/multipart_parser.h" \
       "${__DIR__}/multipart-parser-c/multipart_parser.h" && \
    cp "${__DIR__}/_multipart-parser-c/README.md" \
       "${__DIR__}/multipart-parser-c/README.md" && \
    rm -rf "${__DIR__}/_multipart-parser-c"
    echo "sync multipart-parser-c done"
  fi
fi

cd "${__DIR__}" && \
git add --ignore-errors -A
