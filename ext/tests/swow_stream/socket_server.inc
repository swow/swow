<?php

/**
 * This file is part of Swow
 *
 * @link    https://github.com/swow/swow
 * @contact twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);
// this file can be used without swow:
// php tests/swow_stream/socket_server.inc | php tests/swow_stream/socket_client.inc
require_once __DIR__ . '/../include/bootstrap.php';

const ACCEPT_TIMEOUT = 10;

static $callTime = 0;

function sendFirst(mixed /* resource|false */ $stream): void
{
    global $callTime;
    $callTime++;
    Assert::greaterThan(fwrite($stream, "sendFirst{$callTime}"), 0);
    Assert::same(fread($stream, 1024), "recvFirst{$callTime}");
    fclose($stream);
}

function recvFirst(mixed /* resource|false */ $stream): void
{
    global $callTime;
    $callTime++;
    Assert::same(fread($stream, 1024), "sendFirst{$callTime}");
    Assert::greaterThan(fwrite($stream, "recvFirst{$callTime}"), 0);
    fclose($stream);
}

$paths = testX509Paths(__DIR__ . '/socket_serverX509');
// var_dump(file_get_contents($verydeepCertPath));

function serverWithConfig(array $config): mixed /* resource */
{
    $context = stream_context_create([
        'ssl' => $config,
    ]);
    return stream_socket_server('ssl://127.0.0.1:0', $errno, $errstr, STREAM_SERVER_BIND | STREAM_SERVER_LISTEN, $context);
}

// 1. server with empty array
$server = serverWithConfig([]);
printf("%s\n", stream_socket_get_name($server, false));
Assert::same(@stream_socket_accept($server, ACCEPT_TIMEOUT), false);

// 2. server with valid certificate, client donot accept the ca
$server = serverWithConfig([
    'local_cert' => $paths['localhost']['cert'],
    'local_pk' => $paths['localhost']['key'],
]);
printf("%s\n", stream_socket_get_name($server, false));
Assert::same(@stream_socket_accept($server, ACCEPT_TIMEOUT), false);

// 3. server with valid certificate, client accept the ca
$server = serverWithConfig([
    'local_cert' => $paths['localhost']['cert'],
    'local_pk' => $paths['localhost']['key'],
]);
printf("%s\n", stream_socket_get_name($server, false));
$conn = stream_socket_accept($server, ACCEPT_TIMEOUT);
Assert::notSame($conn, false);
sendFirst($conn);

// 4a. server checks peer name, client donot have cert
// PHP buggy: client will success and server will fail
// $server = serverWithConfig([
//     'cafile' => $paths['ca']['cert'],
//     'local_cert' => $paths['localhost']['cert'],
//     'local_pk' => $paths['localhost']['key'],
//     'verify_peer' => true,
//     'verify_peer_name' => false,
// ]);
// printf("%s\n", stream_socket_get_name($server, false));
// Assert::same(@stream_socket_accept($server, ACCEPT_TIMEOUT), false);

// 4b. server checks peer name, client have bad cert
$server = serverWithConfig([
    'cafile' => $paths['ca']['cert'],
    'local_cert' => $paths['selfsigned']['cert'],
    'local_pk' => $paths['selfsigned']['key'],
    'verify_peer' => true,
    'verify_peer_name' => false,
]);
printf("%s\n", stream_socket_get_name($server, false));
Assert::same(@stream_socket_accept($server, ACCEPT_TIMEOUT), false);

// 4c. server checks peer name, client have cert, but bad peer name
$server = serverWithConfig([
    'cafile' => $paths['ca']['cert'],
    'local_cert' => $paths['selfsigned']['cert'],
    'local_pk' => $paths['selfsigned']['key'],
    'verify_peer' => true,
    'verify_peer_name' => false,
]);
printf("%s\n", stream_socket_get_name($server, false));
Assert::same(@stream_socket_accept($server, ACCEPT_TIMEOUT), false);

// 5. server checks peer name, client have cert
$server = serverWithConfig([
    'cafile' => $paths['ca']['cert'],
    'local_cert' => $paths['localhost']['cert'],
    'local_pk' => $paths['localhost']['key'],
    'verify_peer' => true,
    'verify_peer_name' => false,
]);
printf("%s\n", stream_socket_get_name($server, false));
$conn = stream_socket_accept($server, ACCEPT_TIMEOUT);
Assert::notSame($conn, false);
recvFirst($conn);

// 6. server checks peer name, client have cert
$server = serverWithConfig([
    'cafile' => $paths['ca']['cert'],
    'local_cert' => $paths['localhost']['cert'],
    'local_pk' => $paths['localhost']['key'],
    'verify_peer' => true,
    'verify_peer_name' => true,
]);

// 7. self signed server certificate, client donot accept it
$server = serverWithConfig([
    'local_cert' => $paths['selfsigned']['cert'],
    'local_pk' => $paths['selfsigned']['key'],
    'verify_peer' => false,
]);
printf("%s\n", stream_socket_get_name($server, false));
Assert::same(@stream_socket_accept($server, ACCEPT_TIMEOUT), false);

// 8. self signed server certificate, client accept it
$server = serverWithConfig([
    'local_cert' => $paths['selfsigned']['cert'],
    'local_pk' => $paths['selfsigned']['key'],
    'verify_peer' => false,
]);
printf("%s\n", stream_socket_get_name($server, false));
$conn = stream_socket_accept($server, ACCEPT_TIMEOUT);
sendFirst($conn);

// 9. very deep certificate chain
$server = serverWithConfig([
    'local_cert' => $paths['verydeep']['cert'],
    'local_pk' => $paths['verydeep']['key'],
    'verify_peer' => false,
]);
printf("%s\n", stream_socket_get_name($server, false));
Assert::same(@stream_socket_accept($server, ACCEPT_TIMEOUT), false);

// 10. very deep certificate chain, client accept it
$server = serverWithConfig([
    'cafile' => $paths['ca']['cert'],
    'local_cert' => $paths['verydeep']['cert'],
    'local_pk' => $paths['verydeep']['key'],
    'verify_peer' => false,
]);
printf("%s\n", stream_socket_get_name($server, false));
$conn = stream_socket_accept($server, ACCEPT_TIMEOUT);
recvFirst($conn);

// prepare for peer_fingerprint tests
$clientCert = openssl_x509_read('file://' . $paths['client']['cert']);
$clientFingerprints = [];
foreach (['md5', 'sha1', 'sha256', 'sha512', 'sha3-256', 'sm3', 'blake2b512'] as $algo) {
    if (in_array($algo, openssl_get_md_methods(), true)) {
        $clientFingerprints[$algo] = openssl_x509_fingerprint($clientCert, $algo);
    }
}
$badClientFingerprints = [];
foreach ($clientFingerprints as $algo => $fingerprint) {
    $badClientFingerprints[$algo] = $fingerprint;
    if ($fingerprint[0] === '0') {
        $badClientFingerprints[$algo] = '1' . substr($fingerprint, 1);
    } else {
        $badClientFingerprints[$algo] = '0' . substr($fingerprint, 1);
    }
}
// php will only show warnings, so convert warnings to exceptions
class PHPWarningException extends Exception
{
}
set_error_handler(static function (int $errno, string $errstr): void {
    throw new PHPWarningException($errstr);
});

if (isset($clientFingerprints['md5'])) {
    // 11a. server checks single md5 string fingerprint matched
    $server = serverWithConfig([
        'cafile' => $paths['ca']['cert'],
        'local_cert' => $paths['localhost']['cert'],
        'local_pk' => $paths['localhost']['key'],
        'verify_peer' => true,
        'verify_peer_name' => false,
        'peer_fingerprint' => $clientFingerprints['md5'],
    ]);
    printf("%s\n", stream_socket_get_name($server, false));
    $conn = stream_socket_accept($server, ACCEPT_TIMEOUT);
    Assert::notSame($conn, false);
    recvFirst($conn);

    // 11b. server checks single md5 string fingerprint not matched
    $server = serverWithConfig([
        'cafile' => $paths['ca']['cert'],
        'local_cert' => $paths['localhost']['cert'],
        'local_pk' => $paths['localhost']['key'],
        'verify_peer' => true,
        'verify_peer_name' => false,
        'peer_fingerprint' => $badClientFingerprints['md5'],
    ]);
    printf("%s\n", stream_socket_get_name($server, false));
    Assert::throws(static function () use ($server): void {
        $stream = stream_socket_accept($server, ACCEPT_TIMEOUT);
    }, 'PHPWarningException', expectMessage: '/peer_fingerprint match failure|certificate verify failed/'); // the error message differs

    // 11c. server checks single md5 fingerprint in array matched
    $server = serverWithConfig([
        'cafile' => $paths['ca']['cert'],
        'local_cert' => $paths['localhost']['cert'],
        'local_pk' => $paths['localhost']['key'],
        'verify_peer' => true,
        'verify_peer_name' => false,
        'peer_fingerprint' => [
            'md5' => $clientFingerprints['md5'],
        ],
    ]);
    printf("%s\n", stream_socket_get_name($server, false));
    $conn = stream_socket_accept($server, ACCEPT_TIMEOUT);
    Assert::notSame($conn, false);
    recvFirst($conn);

    // 11d. server checks single md5 fingerprint in array not matched
    $server = serverWithConfig([
        'cafile' => $paths['ca']['cert'],
        'local_cert' => $paths['localhost']['cert'],
        'local_pk' => $paths['localhost']['key'],
        'verify_peer' => true,
        'verify_peer_name' => false,
        'peer_fingerprint' => [
            'md5' => $badClientFingerprints['md5'],
        ],
    ]);
    printf("%s\n", stream_socket_get_name($server, false));
    Assert::throws(static function () use ($server): void {
        stream_socket_accept($server, ACCEPT_TIMEOUT);
    }, 'PHPWarningException', expectMessage: '/peer_fingerprint match failure|certificate verify failed/'); // the error message differs

    // 11e. client checks single md5 string fingerprint matched
    $server = serverWithConfig([
        'cafile' => $paths['ca']['cert'],
        'local_cert' => $paths['localhost']['cert'],
        'local_pk' => $paths['localhost']['key'],
        'verify_peer' => true,
        'verify_peer_name' => false,
    ]);
    printf("%s\n", stream_socket_get_name($server, false));
    $conn = stream_socket_accept($server, ACCEPT_TIMEOUT);
    Assert::notSame($conn, false);
    recvFirst($conn);

    // 11f. client checks single md5 string fingerprint not matched
    $server = serverWithConfig([
        'cafile' => $paths['ca']['cert'],
        'local_cert' => $paths['localhost']['cert'],
        'local_pk' => $paths['localhost']['key'],
        'verify_peer' => true,
        'verify_peer_name' => false,
    ]);
    printf("%s\n", stream_socket_get_name($server, false));
    // quirk here : PHP will success, swow will fail
    Assert::throws(static function () use ($server): void {
        stream_socket_accept($server, ACCEPT_TIMEOUT);
        throw new PHPWarningException('enable crypto failed'); // shim this behavior of PHP
    }, 'PHPWarningException', expectMessage: '/enable crypto failed/');

    // 11g. client checks single md5 fingerprint in array matched
    $server = serverWithConfig([
        'cafile' => $paths['ca']['cert'],
        'local_cert' => $paths['localhost']['cert'],
        'local_pk' => $paths['localhost']['key'],
        'verify_peer' => true,
        'verify_peer_name' => false,
    ]);
    printf("%s\n", stream_socket_get_name($server, false));
    $conn = stream_socket_accept($server, ACCEPT_TIMEOUT);
    Assert::notSame($conn, false);
    recvFirst($conn);

    // 11h. client checks single md5 fingerprint in array not matched
    $server = serverWithConfig([
        'cafile' => $paths['ca']['cert'],
        'local_cert' => $paths['localhost']['cert'],
        'local_pk' => $paths['localhost']['key'],
        'verify_peer' => true,
        'verify_peer_name' => false,
    ]);
    printf("%s\n", stream_socket_get_name($server, false));
    // quirk here: PHP will success, swow will fail
    Assert::throws(static function () use ($server): void {
        stream_socket_accept($server, ACCEPT_TIMEOUT);
        throw new PHPWarningException('enable crypto failed'); // shim this behavior of PHP
    }, 'PHPWarningException', expectMessage: '/enable crypto failed/');
}

// 12a. server checks single sha1 string fingerprint matched
$server = serverWithConfig([
    'cafile' => $paths['ca']['cert'],
    'local_cert' => $paths['localhost']['cert'],
    'local_pk' => $paths['localhost']['key'],
    'verify_peer' => true,
    'verify_peer_name' => false,
    'peer_fingerprint' => $clientFingerprints['sha1'],
]);
printf("%s\n", stream_socket_get_name($server, false));
$conn = stream_socket_accept($server, ACCEPT_TIMEOUT);
Assert::notSame($conn, false);
recvFirst($conn);

// 12b. server checks single sha1 string fingerprint not matched
$server = serverWithConfig([
    'cafile' => $paths['ca']['cert'],
    'local_cert' => $paths['localhost']['cert'],
    'local_pk' => $paths['localhost']['key'],
    'verify_peer' => true,
    'verify_peer_name' => false,
    'peer_fingerprint' => $badClientFingerprints['sha1'],
]);
printf("%s\n", stream_socket_get_name($server, false));
Assert::throws(static function () use ($server): void {
    stream_socket_accept($server, ACCEPT_TIMEOUT);
}, 'PHPWarningException', expectMessage: '/peer_fingerprint match failure|certificate verify failed/'); // the error message differs

// 12c. server checks single sha1 fingerprint in array matched
$server = serverWithConfig([
    'cafile' => $paths['ca']['cert'],
    'local_cert' => $paths['localhost']['cert'],
    'local_pk' => $paths['localhost']['key'],
    'verify_peer' => true,
    'verify_peer_name' => false,
    'peer_fingerprint' => [
        'sha1' => $clientFingerprints['sha1'],
    ],
]);
printf("%s\n", stream_socket_get_name($server, false));
$conn = stream_socket_accept($server, ACCEPT_TIMEOUT);
Assert::notSame($conn, false);
recvFirst($conn);

// 12d. server checks single sha1 fingerprint in array not matched
$server = serverWithConfig([
    'cafile' => $paths['ca']['cert'],
    'local_cert' => $paths['localhost']['cert'],
    'local_pk' => $paths['localhost']['key'],
    'verify_peer' => true,
    'verify_peer_name' => false,
    'peer_fingerprint' => [
        'sha1' => $badClientFingerprints['sha1'],
    ],
]);
printf("%s\n", stream_socket_get_name($server, false));
Assert::throws(static function () use ($server): void {
    stream_socket_accept($server, ACCEPT_TIMEOUT);
}, 'PHPWarningException', expectMessage: '/peer_fingerprint match failure|certificate verify failed/'); // the error message differs

// 12e. client checks single sha1 string fingerprint matched
$server = serverWithConfig([
    'cafile' => $paths['ca']['cert'],
    'local_cert' => $paths['localhost']['cert'],
    'local_pk' => $paths['localhost']['key'],
    'verify_peer' => true,
    'verify_peer_name' => false,
]);
printf("%s\n", stream_socket_get_name($server, false));
$conn = stream_socket_accept($server, ACCEPT_TIMEOUT);
Assert::notSame($conn, false);
recvFirst($conn);

// 12f. client checks single sha1 string fingerprint not matched
$server = serverWithConfig([
    'cafile' => $paths['ca']['cert'],
    'local_cert' => $paths['localhost']['cert'],
    'local_pk' => $paths['localhost']['key'],
    'verify_peer' => true,
    'verify_peer_name' => false,
]);
printf("%s\n", stream_socket_get_name($server, false));
// quirk here: PHP will success, swow will fail
Assert::throws(static function () use ($server): void {
    stream_socket_accept($server, ACCEPT_TIMEOUT);
    throw new PHPWarningException('enable crypto failed'); // shim this behavior of PHP
}, 'PHPWarningException', expectMessage: '/enable crypto failed/');

// 12g. client checks single sha1 fingerprint in array matched
$server = serverWithConfig([
    'cafile' => $paths['ca']['cert'],
    'local_cert' => $paths['localhost']['cert'],
    'local_pk' => $paths['localhost']['key'],
    'verify_peer' => true,
    'verify_peer_name' => false,
]);
printf("%s\n", stream_socket_get_name($server, false));
$conn = stream_socket_accept($server, ACCEPT_TIMEOUT);
Assert::notSame($conn, false);
recvFirst($conn);

// 12h. client checks single sha1 fingerprint in array not matched
$server = serverWithConfig([
    'cafile' => $paths['ca']['cert'],
    'local_cert' => $paths['localhost']['cert'],
    'local_pk' => $paths['localhost']['key'],
    'verify_peer' => true,
    'verify_peer_name' => false,
]);
printf("%s\n", stream_socket_get_name($server, false));
// quirk here
Assert::throws(static function () use ($server): void {
    stream_socket_accept($server, ACCEPT_TIMEOUT);
    throw new PHPWarningException('enable crypto failed'); // shim this behavior of PHP
}, 'PHPWarningException', expectMessage: '/enable crypto failed/');

// 13a. server checks multiple fingerprints matched
$server = serverWithConfig([
    'cafile' => $paths['ca']['cert'],
    'local_cert' => $paths['localhost']['cert'],
    'local_pk' => $paths['localhost']['key'],
    'verify_peer' => true,
    'verify_peer_name' => false,
    'peer_fingerprint' => [
        'sha1' => $clientFingerprints['sha1'],
        'sha256' => $clientFingerprints['sha256'],
    ],
]);
printf("%s\n", stream_socket_get_name($server, false));
$conn = stream_socket_accept($server, ACCEPT_TIMEOUT);
Assert::notSame($conn, false);
recvFirst($conn);

// 13b. server checks multiple fingerprints all not matched
$server = serverWithConfig([
    'cafile' => $paths['ca']['cert'],
    'local_cert' => $paths['localhost']['cert'],
    'local_pk' => $paths['localhost']['key'],
    'verify_peer' => true,
    'verify_peer_name' => false,
    'peer_fingerprint' => [
        'sha1' => $badClientFingerprints['sha1'],
        'sha256' => $badClientFingerprints['sha256'],
    ],
]);
printf("%s\n", stream_socket_get_name($server, false));
Assert::throws(static function () use ($server): void {
    stream_socket_accept($server, ACCEPT_TIMEOUT);
}, 'PHPWarningException', expectMessage: '/peer_fingerprint match failure|certificate verify failed/'); // the error message differs

// 13c. server checks multiple fingerprints partially matched
$server = serverWithConfig([
    'cafile' => $paths['ca']['cert'],
    'local_cert' => $paths['localhost']['cert'],
    'local_pk' => $paths['localhost']['key'],
    'verify_peer' => true,
    'verify_peer_name' => false,
    'peer_fingerprint' => [
        'sha1' => $clientFingerprints['sha1'],
        'sha256' => $badClientFingerprints['sha256'],
    ],
]);
printf("%s\n", stream_socket_get_name($server, false));
Assert::throws(static function () use ($server): void {
    stream_socket_accept($server, ACCEPT_TIMEOUT);
}, 'PHPWarningException', expectMessage: '/peer_fingerprint match failure|certificate verify failed/'); // the error message differs

// 13d. client checks multiple fingerprints all supported matched
$server = serverWithConfig([
    'cafile' => $paths['ca']['cert'],
    'local_cert' => $paths['localhost']['cert'],
    'local_pk' => $paths['localhost']['key'],
    'verify_peer' => true,
    'verify_peer_name' => false,
    'peer_fingerprint' => $clientFingerprints,
]);
printf("%s\n", stream_socket_get_name($server, false));
$conn = stream_socket_accept($server, ACCEPT_TIMEOUT);
Assert::notSame($conn, false);
sendFirst($conn);

// 13e. server checks multiple fingerprints matched
$server = serverWithConfig([
    'cafile' => $paths['ca']['cert'],
    'local_cert' => $paths['localhost']['cert'],
    'local_pk' => $paths['localhost']['key'],
    'verify_peer' => true,
    'verify_peer_name' => false,
]);
printf("%s\n", stream_socket_get_name($server, false));
$conn = stream_socket_accept($server, ACCEPT_TIMEOUT);
Assert::notSame($conn, false);
recvFirst($conn);

// 13f. server checks multiple fingerprints all not matched
$server = serverWithConfig([
    'cafile' => $paths['ca']['cert'],
    'local_cert' => $paths['localhost']['cert'],
    'local_pk' => $paths['localhost']['key'],
    'verify_peer' => true,
    'verify_peer_name' => false,
]);
printf("%s\n", stream_socket_get_name($server, false));
// quirk here: PHP will success, swow will fail
Assert::throws(static function () use ($server): void {
    stream_socket_accept($server, ACCEPT_TIMEOUT);
    throw new PHPWarningException('enable crypto failed'); // shim this behavior of PHP
}, 'PHPWarningException', expectMessage: '/enable crypto failed/');

// 13g. server checks multiple fingerprints partially matched
$server = serverWithConfig([
    'cafile' => $paths['ca']['cert'],
    'local_cert' => $paths['localhost']['cert'],
    'local_pk' => $paths['localhost']['key'],
    'verify_peer' => true,
    'verify_peer_name' => false,
]);
printf("%s\n", stream_socket_get_name($server, false));
// quirk here: PHP will success, swow will fail
Assert::throws(static function () use ($server): void {
    stream_socket_accept($server, ACCEPT_TIMEOUT);
    throw new PHPWarningException('enable crypto failed'); // shim this behavior of PHP
}, 'PHPWarningException', expectMessage: '/enable crypto failed/');

// 13h. client checks multiple fingerprints all supported matched
$server = serverWithConfig([
    'cafile' => $paths['ca']['cert'],
    'local_cert' => $paths['localhost']['cert'],
    'local_pk' => $paths['localhost']['key'],
    'verify_peer' => true,
    'verify_peer_name' => false,
    'peer_fingerprint' => $clientFingerprints,
]);
printf("%s\n", stream_socket_get_name($server, false));
$conn = stream_socket_accept($server, ACCEPT_TIMEOUT);
Assert::notSame($conn, false);
sendFirst($conn);

printf("server end\n");
