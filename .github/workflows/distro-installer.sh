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
    # enable codeready for el 8
    dnf config-manager --set-enabled codeready-builder-for-rhel-8-x86_64-rpms ||
    # enable powertools for el 8 forks
    for fn in \
        /etc/yum.repos.d/CentOS-Linux-PowerTools.repo \
        /etc/yum.repos.d/CentOS-PowerTools.repo \
        /etc/yum.repos.d/almalinux-powertools.repo \
        /etc/yum.repos.d/Rocky-PowerTools.repo
    do
        [ -f "$fn" ] &&
        sed -i 's/^enabled=0$/enabled=1/g' $fn
    done
    yum update -yy
    # enable epel for el 7 things
    yum install -yy epel-release
    yum update -yy
    yum groupinstall -yy "Development Tools"
    yum install -yy sqlite-devel libxml2-devel libcurl-devel openssl-devel re2c bison
}

installdnf()
{
    info Installing dependencies using dnf
    dnf update -yy
    openssl_pkg="openssl-devel"
    openssl_ver=$(dnf list -qq openssl-devel | awk '(NR==2){split($2,a,":");split(a[2],b,".");print(b[1])}')
    if [ "$openssl_ver" = "3" ] && [ "${SUPPORTS_OPENSSL3}" != "yes" ]
    then
        # rawhide is now using openssl 3
        # so it's need to be downgraded when building php 8.0
        openssl_pkg="openssl1.1-devel"
    fi

    # for el8 things
    if dnf install -yy epel-release 'dnf-command(config-manager)'
    then
        dnf config-manager --enable epel
        dnf config-manager --enable powertools 
    fi

    dnf groupinstall -yy "Development Tools"
    dnf install -yy sqlite-devel libxml2-devel libcurl-devel ${openssl_pkg} re2c bison autoconf
}

installapt()
{
    info Installing dependencies using api
    apt-get update -yyq
    apt-get install --no-install-recommends -yyq libsqlite3-dev libxml2-dev libcurl4-openssl-dev libssl-dev build-essential re2c bison autoconf pkgconf
}

installpacman()
{
    info Installing dependencies using pacman
    pacman -Syyu --noconfirm --noprogressbar
    pacman -S --noconfirm --noprogressbar sqlite libxml2 curl openssl base-devel re2c bison
}

installapk()
{
    info Installing dependencies using apk
    apk update
    apk add --no-progress sqlite-dev libxml2-dev curl-dev alpine-sdk autoconf automake re2c bison
}

installzypper()
{
    info Installing dependencies using zypper
    zypper --non-interactive update
    zypper --non-interactive install sqlite3-devel libxml2-devel libcurl-devel libopenssl-devel patterns-devel-C-C++-devel_C_C++ re2c bison
}

buildphp()
{

    cd "${PHP_SRC}"
    if [ ! -f ./configure ]
    then
        info Autoconf PHP
        ./buildconf --force
    fi
    info Configuring PHP
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

    if [ -f "${PHP_SRC}/ext/openssl/php_openssl.h" ] &&
        [ -n "$(grep -e '#define PHP_OPENSSL_API_VERSION 0x30000' "${PHP_SRC}/ext/openssl/php_openssl.h")" ]
    then
        info this version of PHP supports OpenSSL 3
        SUPPORTS_OPENSSL3="yes"
    fi

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
    exec "$PREFIX/bin/php" "${SWOW_SRC}/tools/test-extension.php"
}

mian "$1"
