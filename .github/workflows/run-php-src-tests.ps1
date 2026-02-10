
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
$PSNativeCommandUseErrorActionPreference = $true

if ( "${env:GITHUB_WORKSPACE}" -Eq "" ) {
    Write-Host "This script is intended to run in GitHub Actions environment only."
    Write-Host "It's dangerous and meaningless in other environments."
    exit 1
}

$phpSrcRepo = "https://github.com/php/php-src.git"
if ( "${env:PHP_SRC_REPO}" -Ne "" ) {
    $phpSrcRepo = "${env:PHP_SRC_REPO}"
}

$phpVersion = php -r "echo PHP_VERSION;"
if ( "$phpVersion" -Match '-dev$') {
    $phpBranch = "master"
} else {
    $phpBranch = "php-$phpVersion"
}

$includeTests = @(
    "sapi/cli/tests",
    "tests/basic",
    "tests/output",
    "tests/security",
    "ext/curl/tests",
    "ext/opcache/tests",
    "ext/openssl/tests",
    "ext/posix/tests",
    "ext/pcntl/tests",
    "ext/sockets/tests"
)

$excludeTests = @(
    # hard coded object number
    # fixme: can this be fixed?
    "ext/opcache/tests/bug78986.phpt",
    "ext/opcache/tests/bug79535.phpt",
    "ext/opcache/tests/jit/assign_042.phpt",
    "ext/opcache/tests/jit/assign_056.phpt",
    "ext/opcache/tests/jit/assign_obj_op_002.phpt",
    "ext/opcache/tests/jit/assign_obj_op_003.phpt",
    "ext/opcache/tests/jit/assign_obj_ref_001.phpt",
    "ext/opcache/tests/jit/closure_001.phpt",
    "ext/opcache/tests/jit/fetch_obj_003.phpt",
    "ext/opcache/tests/jit/fetch_obj_004.phpt",
    "ext/opcache/tests/jit/fetch_obj_007.phpt",
    "ext/opcache/tests/jit/gh12482.phpt",
    "ext/opcache/tests/jit/mod_003.phpt",
    "ext/opcache/tests/jit/mod_005.phpt",
    "ext/opcache/tests/jmp_elim_004.phpt",
    "ext/sockets/tests/socket_create_pair.phpt",
    "ext/sockets/tests/socket_addrinfo_bind.phpt",
    "ext/sockets/tests/socket_addrinfo_connect.phpt",
    "ext/sockets/tests/socket_set_nonblock.phpt",
    "ext/openssl/tests/bug81713.phpt",
    "ext/openssl/tests/ecc.phpt",
    "ext/openssl/tests/openssl_cms_decrypt_error.phpt",
    "ext/openssl/tests/openssl_pkcs7_decrypt_error.phpt",
    "ext/openssl/tests/openssl_x509_free_basic.phpt",
    "ext/curl/tests/curl_share_close_basic001.phpt",
    "ext/curl/tests/curl_multi_init_basic.phpt",
    "ext/curl/tests/curl_multi_close_basic001.phpt",
    "ext/curl/tests/curl_multi_close_basic.phpt",
    "ext/curl/tests/curl_int_cast.phpt",
    "ext/curl/tests/curl_close_basic.phpt",
    "ext/curl/tests/curl_basic_014.phpt",
    "ext/curl/tests/bug72202.phpt",
    "ext/curl/tests/bug48514.phpt",
    # error message changed
    "ext/curl/tests/curl_basic_028.phpt",
    "ext/openssl/tests/bug54992.phpt", # fixme: bad message
    "ext/openssl/tests/bug65538_002.phpt",
    "ext/openssl/tests/bug65729.phpt", # fixme: bad message
    "ext/openssl/tests/bug68920.phpt",
    "ext/openssl/tests/gh9310.phpt",
    "ext/openssl/tests/san_peer_matching.phpt", # fixme: bad message
    "ext/openssl/tests/stream_verify_peer_name_003.phpt", # fixme: bad message
    "ext/curl/tests/bug78775.phpt",
    # no readline support
    "sapi/cli/tests/009.phpt",
    "sapi/cli/tests/012-2.phpt",
    # output changes
    "ext/curl/tests/check_win_config.phpt"
)

$nproc = Get-CIMInstance -ClassName Win32_ComputerSystem | Select-Object -ExpandProperty NumberOfLogicalProcessors

# clone php-src
& git "clone" `
    "--single-branch" `
    "--branch" "${phpBranch}" `
    "--depth" "1" `
    "${phpSrcRepo}" `
    "php-src"

# remove excluded tests
Set-Location -Path php-src
foreach ($excludeTest in $excludeTests) {
    Remove-Item -Path $excludeTest -Recurse -Force -ErrorAction SilentlyContinue
}

# no pty for Windows
${env:SKIP_IO_CAPTURE_TESTS} = "true"

# test it
& php "run-tests.php" `
    "-q" `
    "-d" "extension=swow" `
    "-d" "opcache.enable_cli=on" `
    "--show-diff" `
    "--show-slow" "3000" `
    "--set-timeout" "30" `
    "--color" `
    "-j$nproc" `
    ${includeTests}
