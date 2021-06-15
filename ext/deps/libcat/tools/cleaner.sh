#!/bin/bash
__DIR__=$(cd "$(dirname "$0")" || exit 1; pwd); [ -z "${__DIR__}" ] && exit 1

error(){ echo "[ERROR] $1"; exit 1; }
success(){ echo "[SUCCESS] $1"; exit 0; }
info(){ echo "[INFO] $1";}

workdir="$1"

if ! cd "${workdir}"; then
  error "Cd to ${workdir} failed"
fi

info "Scanning dir \"${workdir}\" ..."

if [ ! -f "./CMakeLists.txt" ] && \
   [ ! -f "./config.m4" ] && \
   [ ! -f "./config.w32" ] && \
   { [ ! -d "./src" ] || [ ! -d "./include" ]; } then
  error "Non-project dir ${workdir}"
fi

if [ -z "${USE_AUTOTOOLS}" ]; then
  if [ -f "./CMakeLists.txt" ]; then
    USE_AUTOTOOLS=0
  elif [ -f "./config.m4" ] || [ -f "./config.w32" ] ; then
    USE_AUTOTOOLS=1
  else
    error "Unknown project type"
  fi
fi

info "USE_AUTOTOOLS=${USE_AUTOTOOLS}"

if [ "${USE_AUTOTOOLS}" != "1" ] && [ -f "./build/CMakeCache.txt" ]; then
  info "CMake build dir will be removed:"
  rm -rf -v ./build
fi

info "Following files will be removed:"

if [ "${USE_AUTOTOOLS}" = "1" ]; then
  find . \( -path "*/build" -prune -name "skip build dir" -o -name \*.gcno -o -name \*.gcda            -follow \) -print0 | xargs -0 rm -f -v
  find . \( -path "*/build" -prune -name "skip build dir" -o -name \*.lo -o -name \*.o -o -name \*.dep -follow \) -print0 | xargs -0 rm -f -v
  find . \( -path "*/build" -prune -name "skip build dir" -o -name \*.la -o -name \*.a                 -follow \) -print0 | xargs -0 rm -f -v
  find . \( -path "*/build" -prune -name "skip build dir" -o -name \*.so                               -follow \) -print0 | xargs -0 rm -f -v
  find . \( -path "*/build" -prune -name "skip build dir" -o -name .libs -a -type d                    -follow \) -print0 | xargs -0 rm -rf -v
else
  find . \( -name main -a -type f -follow \) -print0 | xargs -0 rm -f -v
fi

success "Clean '${workdir}' done"
