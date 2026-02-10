#!/usr/bin/env bash

set -xeo pipefail

if [[ -z "${GITHUB_WORKSPACE}" ]]; then
    echo "This script is intended to run in GitHub Actions environment only."
    echo "It's dangerous and meaningless in other environments."
    exit 1
fi

PHP_SRC_REPO="${PHP_SRC_REPO:-https://github.com/php/php-src.git}"

PHP_VERSION="$(php -r 'echo PHP_VERSION;')"
PHP_BRANCH="php-${PHP_VERSION}"
if [[ "${PHP_VERSION%%-dev}" != "${PHP_VERSION}" ]]; then
    PHP_BRANCH=master
fi

INCLUDE_TESTS=(
    sapi/cli/tests
    tests/basic
    tests/output
    tests/security
    ext/curl/tests
    ext/opcache/tests
    ext/openssl/tests
    ext/posix/tests
    ext/pcntl/tests
    ext/sockets/tests
)

EXCLUDE_TESTS=(
    # hard coded object number
    # fixme: can this be fixed?
    ext/opcache/tests/bug78986.phpt
    ext/opcache/tests/bug79535.phpt
    ext/opcache/tests/jit/assign_042.phpt
    ext/opcache/tests/jit/assign_056.phpt
    ext/opcache/tests/jit/assign_obj_op_002.phpt
    ext/opcache/tests/jit/assign_obj_op_003.phpt
    ext/opcache/tests/jit/assign_obj_ref_001.phpt
    ext/opcache/tests/jit/closure_001.phpt
    ext/opcache/tests/jit/fetch_obj_003.phpt
    ext/opcache/tests/jit/fetch_obj_004.phpt
    ext/opcache/tests/jit/fetch_obj_007.phpt
    ext/opcache/tests/jit/gh12482.phpt
    ext/opcache/tests/jit/mod_003.phpt
    ext/opcache/tests/jit/mod_005.phpt
    ext/opcache/tests/jmp_elim_004.phpt
    ext/sockets/tests/socket_create_pair.phpt
    ext/sockets/tests/socket_addrinfo_bind.phpt
    ext/sockets/tests/socket_addrinfo_connect.phpt
    ext/sockets/tests/socket_set_nonblock.phpt
    ext/openssl/tests/bug81713.phpt
    ext/openssl/tests/ecc.phpt
    ext/openssl/tests/openssl_cms_decrypt_error.phpt
    ext/openssl/tests/openssl_pkcs7_decrypt_error.phpt
    ext/openssl/tests/openssl_x509_free_basic.phpt
    ext/curl/tests/curl_share_close_basic001.phpt
    ext/curl/tests/curl_multi_init_basic.phpt
    ext/curl/tests/curl_multi_close_basic001.phpt
    ext/curl/tests/curl_multi_close_basic.phpt
    ext/curl/tests/curl_int_cast.phpt
    ext/curl/tests/curl_close_basic.phpt
    ext/curl/tests/curl_basic_014.phpt
    ext/curl/tests/bug72202.phpt
    ext/curl/tests/bug48514.phpt
    # error message changed
    ext/openssl/tests/bug54992.phpt # fixme: bad message
    ext/openssl/tests/bug65538_002.phpt
    ext/openssl/tests/bug65729.phpt # fixme: bad message
    ext/openssl/tests/bug68920.phpt
    ext/openssl/tests/gh9310.phpt
    ext/openssl/tests/san_peer_matching.phpt # fixme: bad message
    ext/openssl/tests/stream_verify_peer_name_003.phpt # fixme: bad message
    ext/curl/tests/bug78775.phpt
    # no readline support
    sapi/cli/tests/009.phpt
    sapi/cli/tests/012-2.phpt
)

if [[ "$(uname -s)" == "Darwin" ]]
then
    NPROC=$(sysctl -n hw.ncpu)
else
    NPROC=$(nproc)
fi

# clone php-src
git clone \
    --single-branch \
    --branch "${PHP_BRANCH}" \
    --depth 1 \
    "$PHP_SRC_REPO" \
    php-src

# remove excluded tests
cd php-src
rm -rf "${EXCLUDE_TESTS[@]}"

# test it
php run-tests.php \
    -q \
    -d extension="${GITHUB_WORKSPACE}/ext/.libs/swow.so" \
    -d opcache.enable_cli=on \
    --show-diff \
    --show-slow 3000 \
    --set-timeout 30 \
    --color \
    "-j$NPROC" \
    "${INCLUDE_TESTS[@]}"
