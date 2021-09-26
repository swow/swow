#!/bin/sh
#      ^ should be bash, but this is also for ash
# install php+swow for different distros in docker
# shellcheck shell=bash

set -eo pipefail

PHP_SRC=${PHP_SRC-/src/php-src}
SWOW_SRC=${SWOW_SRC-/src/swow}
PREFIX=${PREFIX-/inst}

info()
{
    printf ${CI+"::group::"}"\033[1m[IFO]\033[0m $*\n"
}

err()
{
    printf "\033[31;1m[IFO]\033[0m $*\n"
    exit 1
}

installyum()
{
    info Installing dependencies using yum
    yum update -yy
    yum groupinstall -yy "Development Tools"
    yum install -yy sqlite-devel libxml2-devel libcurl-devel openssl-devel
}

installdnf()
{
    info Installing dependencies using dnf
    dnf update -yy
    dnf groupinstall -yy "Development Tools"
    dnf install -yy sqlite-devel libxml2-devel libcurl-devel openssl-devel
}

installapt()
{
    info Installing dependencies using api
    apt-get update -yyq
    apt-get install --no-install-recommends -yyq libsqlite3-dev libxml2-dev libcurl4-openssl-dev libssl-dev build-essential
}

installpacman()
{
    info Installing dependencies using pacman
    pacman -Syyu --noconfirm --noprogressbar
    pacman -S --noconfirm --noprogressbar sqlite libxml2 curl openssl base-devel
}

installapk()
{
    info Installing dependencies using apk
    apk update
    apk add --no-progress sqlite-dev libxml2-dev curl-dev alpine-sdk autoconf automake
}

installzypper()
{
    info Installing dependencies using zypper
    zypper --non-interactive update
    zypper --non-interactive install sqlite3-devel libxml2-devel libcurl-devel libopenssl-devel patterns-devel-C-C++-devel_C_C++
}

buildphp()
{
    info Configuring PHP
    cd "${PHP_SRC}"
    ./configure --disable-phpdbg --disable-cgi --disable-fpm --prefix="${PREFIX}" --with-openssl --with-curl
    info Building PHP
    make -j"${cpunum-2}"
    info Installing PHP
    make install
}

buildswow()
{
    info Configuring Swow
    cd "${SWOW_SRC}/ext"
    phpize
    ./configure --enable-swow-debug --enable-swow-curl --enable-swow-ssl
    info Building Swow
    make -j"${cpunum-2}" EXTRA_CFLAGS='-O2' >/tmp/build.log 2>/tmp/buildwarn.log || {
        [ "$CI" = true ] && printf '::error::'
        cat /tmp/buildwarn.log
        exit 1
    }
    [ -s /tmp/buildwarn.log ] && {
        [ "$CI" = true ] && printf '::warning::'
        cat /tmp/buildwarn.log
    }
    info Installing Swow
    make install
}

mian()
{
    # install dependencies
    case $1 in
        "yum") installyum ;;
        "pacman") installpacman ;;
        "dnf") installdnf ;;
        "zypper") installzypper ;;
        "apk") installapk ;;
        "apt") installapt ;;
        *) err "Unsupported pkg manager $1" ;;
    esac
    # do builds
    type nproc 2>/tmp/log >/tmp/log && cpunum=$(nproc)
    export PATH=${PREFIX}/bin:$PATH
    buildphp
    buildswow
    info Show extension installed
    "$PREFIX/bin/php" -dextension=swow --ri swow
    info Run extension tests
    exec "$PREFIX/bin/php" -n "${SWOW_SRC}/ext/tests/runner/run-tests.php" \
        --color \
        -d extension=swow \
        -j"${cpunum-2}" \
        --show-diff \
        --set-timeout 30 \
        "${SWOW_SRC}/ext/tests"
}

mian "$1"
