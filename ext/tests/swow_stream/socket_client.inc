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

const CONNECT_TIMEOUT = 10;

$callTime = 0;

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

$paths = testX509Paths(__DIR__ . '/socket_clientX509');

function clientWithConfig(string $serverAddr, array $config, bool $expectSuccess = true): mixed /* resource|false */
{
    $context = stream_context_create([
        'ssl' => $config,
    ]);
    if ($expectSuccess) {
        $client = stream_socket_client("ssl://{$serverAddr}", $errno, $errstr, CONNECT_TIMEOUT, STREAM_CLIENT_CONNECT, $context);
        Assert::notSame($client, false);
    } else {
        $client = @stream_socket_client("ssl://{$serverAddr}", $errno, $errstr, CONNECT_TIMEOUT, STREAM_CLIENT_CONNECT, $context);
        Assert::same($client, false);
    }
    return $client;
}

// 1. client with empty array
$client = clientWithConfig(fgets(STDIN), [], false);

// 2. server with valid certificate, client donot accept the ca
$client = clientWithConfig(fgets(STDIN), [], false);

// 3. server with valid certificate, client accept the ca
$client = clientWithConfig(fgets(STDIN), [
    'cafile' => $paths['ca']['cert'],
    'peer_name' => 'localhost',
    'verify_peer' => true,
]);
recvFirst($client);

// 4a. server checks peer name, client donot have cert
// PHP buggy: client will success and server will fail
// $client = clientWithConfig(fgets(STDIN), [
//     'cafile' => $paths['ca']['cert'],
//     'verify_peer' => true,
//     'verify_peer_name' => false,
// ], false);

// 4b. server checks peer name, client have bad cert
$client = clientWithConfig(fgets(STDIN), [
    'cafile' => $paths['ca']['cert'],
    'local_cert' => $paths['selfsigned']['cert'],
    'local_pk' => $paths['selfsigned']['key'],
    'verify_peer' => true,
    'verify_peer_name' => false,
], false);

// 4c. server checks peer name, client have cert, but bad peer name
$client = clientWithConfig(fgets(STDIN), [
    'cafile' => $paths['ca']['cert'],
    'local_cert' => $paths['localhost']['cert'],
    'local_pk' => $paths['localhost']['key'],
    'verify_peer' => true,
    'verify_peer_name' => false,
], false);

// 5. server checks peer name, client have cert
$client = clientWithConfig(fgets(STDIN), [
    'cafile' => $paths['ca']['cert'],
    'peer_name' => 'localhost',
    'verify_peer' => true,
    'verify_peer_name' => true,
    'local_cert' => $paths['client']['cert'],
    'local_pk' => $paths['client']['key'],
]);
sendFirst($client);

// 7. self signed server certificate, client donot accept it
$client = clientWithConfig(fgets(STDIN), [
    'peer_name' => 'localhost',
    'verify_peer' => true,
    'verify_peer_name' => true,
], false);

// 8. self signed server certificate, client accept it
$client = clientWithConfig(fgets(STDIN), [
    'peer_name' => 'localhost',
    'verify_peer' => true,
    'verify_peer_name' => true,
    'allow_self_signed' => true,
]);
recvFirst($client);

// 9. very deep certificate chain
$client = clientWithConfig(fgets(STDIN), [
    'cafile' => $paths['ca']['cert'],
    'peer_name' => 'localhost',
    'verify_peer' => true,
    'verify_peer_name' => true,
], false);

// 10. very deep certificate chain, client accept it
$client = clientWithConfig(fgets(STDIN), [
    'cafile' => $paths['ca']['cert'],
    'peer_name' => 'localhost',
    'verify_peer' => true,
    'verify_peer_name' => true,
    'verify_depth' => 99,
]);
sendFirst($client);

// prepare for peer_fingerprint tests
$serverCert = openssl_x509_read('file://' . $paths['localhost']['cert']);
$serverFingerprints = [];
foreach (['md5', 'sha1', 'sha256', 'sha512', 'sha3-256', 'sm3', 'blake2b512'] as $algo) {
    if (in_array($algo, openssl_get_md_methods(), true)) {
        $serverFingerprints[$algo] = openssl_x509_fingerprint($serverCert, $algo);
    }
}
$badServerFingerprints = [];
foreach ($serverFingerprints as $algo => $fingerprint) {
    $badServerFingerprints[$algo] = $fingerprint;
    if ($fingerprint[0] === '0') {
        $badServerFingerprints[$algo] = '1' . substr($fingerprint, 1);
    } else {
        $badServerFingerprints[$algo] = '0' . substr($fingerprint, 1);
    }
}
// php will only show warnings, so convert warnings to exceptions
class PHPWarningException extends Exception
{
}
set_error_handler(static function (int $errno, string $errstr): void {
    throw new PHPWarningException($errstr);
});

if (isset($serverFingerprints['md5'])) {
    // 11a. server checks single md5 string fingerprint matched
    $client = clientWithConfig(fgets(STDIN), [
        'cafile' => $paths['ca']['cert'],
        'peer_name' => 'localhost',
        'verify_peer' => true,
        'verify_peer_name' => true,
        'local_cert' => $paths['client']['cert'],
        'local_pk' => $paths['client']['key'],
    ]);
    sendFirst($client);

    // 11b. server checks single md5 string fingerprint not matched
    $client = clientWithConfig(fgets(STDIN), [
        'cafile' => $paths['ca']['cert'],
        'peer_name' => 'localhost',
        'verify_peer' => true,
        'verify_peer_name' => true,
        'local_cert' => $paths['client']['cert'],
        'local_pk' => $paths['client']['key'],
    ]);

    // 11c. server checks single md5 fingerprint in array matched
    $client = clientWithConfig(fgets(STDIN), [
        'cafile' => $paths['ca']['cert'],
        'peer_name' => 'localhost',
        'verify_peer' => true,
        'verify_peer_name' => true,
        'local_cert' => $paths['client']['cert'],
        'local_pk' => $paths['client']['key'],
    ]);
    sendFirst($client);

    // 11d. server checks single md5 fingerprint in array not matched
    $client = clientWithConfig(fgets(STDIN), [
        'cafile' => $paths['ca']['cert'],
        'peer_name' => 'localhost',
        'verify_peer' => true,
        'verify_peer_name' => true,
        'local_cert' => $paths['client']['cert'],
        'local_pk' => $paths['client']['key'],
    ]);

    // 11e. client checks single md5 string fingerprint matched
    $client = clientWithConfig(fgets(STDIN), [
        'cafile' => $paths['ca']['cert'],
        'peer_name' => 'localhost',
        'verify_peer' => true,
        'verify_peer_name' => true,
        'local_cert' => $paths['client']['cert'],
        'local_pk' => $paths['client']['key'],
        'peer_fingerprint' => $serverFingerprints['md5'],
    ]);
    sendFirst($client);

    // 11f. client checks single md5 string fingerprint not matched
    Assert::throws(static function () use ($paths, $badServerFingerprints): void {
        clientWithConfig(fgets(STDIN), [
            'cafile' => $paths['ca']['cert'],
            'peer_name' => 'localhost',
            'verify_peer' => true,
            'verify_peer_name' => true,
            'local_cert' => $paths['client']['cert'],
            'local_pk' => $paths['client']['key'],
            'peer_fingerprint' => $badServerFingerprints['md5'],
        ]);
    }, 'PHPWarningException', expectMessage: '/peer_fingerprint match failure|certificate verify failed/'); // the error message differs

    // 11g. client checks single md5 fingerprint in array matched
    $client = clientWithConfig(fgets(STDIN), [
        'cafile' => $paths['ca']['cert'],
        'peer_name' => 'localhost',
        'verify_peer' => true,
        'verify_peer_name' => true,
        'local_cert' => $paths['client']['cert'],
        'local_pk' => $paths['client']['key'],
        'peer_fingerprint' => [
            'md5' => $serverFingerprints['md5'],
        ],
    ]);
    sendFirst($client);

    // 11h. client checks single md5 fingerprint in array not matched
    Assert::throws(static function () use ($paths, $badServerFingerprints): void {
        clientWithConfig(fgets(STDIN), [
            'cafile' => $paths['ca']['cert'],
            'peer_name' => 'localhost',
            'verify_peer' => true,
            'verify_peer_name' => true,
            'local_cert' => $paths['client']['cert'],
            'local_pk' => $paths['client']['key'],
            'peer_fingerprint' => [
                'md5' => $badServerFingerprints['md5'],
            ],
        ]);
    }, 'PHPWarningException', expectMessage: '/peer_fingerprint match failure|certificate verify failed/'); // the error message differs
}

// 12a. server checks single sha1 string fingerprint matched
$client = clientWithConfig(fgets(STDIN), [
    'cafile' => $paths['ca']['cert'],
    'peer_name' => 'localhost',
    'verify_peer' => true,
    'verify_peer_name' => true,
    'local_cert' => $paths['client']['cert'],
    'local_pk' => $paths['client']['key'],
]);
sendFirst($client);

// 12b. server checks single sha1 string fingerprint not matched
$client = clientWithConfig(fgets(STDIN), [
    'cafile' => $paths['ca']['cert'],
    'peer_name' => 'localhost',
    'verify_peer' => true,
    'verify_peer_name' => true,
    'local_cert' => $paths['client']['cert'],
    'local_pk' => $paths['client']['key'],
]);

// 12c. server checks single sha1 fingerprint in array matched
$client = clientWithConfig(fgets(STDIN), [
    'cafile' => $paths['ca']['cert'],
    'peer_name' => 'localhost',
    'verify_peer' => true,
    'verify_peer_name' => true,
    'local_cert' => $paths['client']['cert'],
    'local_pk' => $paths['client']['key'],
]);
sendFirst($client);

// 12d. server checks single sha1 fingerprint in array not matched
$client = clientWithConfig(fgets(STDIN), [
    'cafile' => $paths['ca']['cert'],
    'peer_name' => 'localhost',
    'verify_peer' => true,
    'verify_peer_name' => true,
    'local_cert' => $paths['client']['cert'],
    'local_pk' => $paths['client']['key'],
]);

// 12e. client checks single sha1 string fingerprint matched
$client = clientWithConfig(fgets(STDIN), [
    'cafile' => $paths['ca']['cert'],
    'peer_name' => 'localhost',
    'verify_peer' => true,
    'verify_peer_name' => true,
    'local_cert' => $paths['client']['cert'],
    'local_pk' => $paths['client']['key'],
    'peer_fingerprint' => $serverFingerprints['sha1'],
]);
sendFirst($client);

// 12f. client checks single sha1 string fingerprint not matched
Assert::throws(static function () use ($paths, $badServerFingerprints): void {
    clientWithConfig(fgets(STDIN), [
        'cafile' => $paths['ca']['cert'],
        'peer_name' => 'localhost',
        'verify_peer' => true,
        'verify_peer_name' => true,
        'local_cert' => $paths['client']['cert'],
        'local_pk' => $paths['client']['key'],
        'peer_fingerprint' => $badServerFingerprints['sha1'],
    ]);
}, 'PHPWarningException', expectMessage: '/peer_fingerprint match failure|certificate verify failed/'); // the error message differs

// 12g. client checks single sha1 fingerprint in array matched
$client = clientWithConfig(fgets(STDIN), [
    'cafile' => $paths['ca']['cert'],
    'peer_name' => 'localhost',
    'verify_peer' => true,
    'verify_peer_name' => true,
    'local_cert' => $paths['client']['cert'],
    'local_pk' => $paths['client']['key'],
    'peer_fingerprint' => [
        'sha1' => $serverFingerprints['sha1'],
    ],
]);
sendFirst($client);

// 12h. client checks single sha1 fingerprint in array not matched
Assert::throws(static function () use ($paths, $badServerFingerprints): void {
    clientWithConfig(fgets(STDIN), [
        'cafile' => $paths['ca']['cert'],
        'peer_name' => 'localhost',
        'verify_peer' => true,
        'verify_peer_name' => true,
        'local_cert' => $paths['client']['cert'],
        'local_pk' => $paths['client']['key'],
        'peer_fingerprint' => [
            'sha1' => $badServerFingerprints['sha1'],
        ],
    ]);
}, 'PHPWarningException', expectMessage: '/peer_fingerprint match failure|certificate verify failed/'); // the error message differs

// 13a. server checks multiple fingerprints matched
$client = clientWithConfig(fgets(STDIN), [
    'cafile' => $paths['ca']['cert'],
    'peer_name' => 'localhost',
    'verify_peer' => true,
    'verify_peer_name' => true,
    'local_cert' => $paths['client']['cert'],
    'local_pk' => $paths['client']['key'],
]);
sendFirst($client);

// 13b. server checks multiple fingerprints not matched
$client = clientWithConfig(fgets(STDIN), [
    'cafile' => $paths['ca']['cert'],
    'peer_name' => 'localhost',
    'verify_peer' => true,
    'verify_peer_name' => true,
    'local_cert' => $paths['client']['cert'],
    'local_pk' => $paths['client']['key'],
]);

// 13c. server checks multiple fingerprints partially matched
$client = clientWithConfig(fgets(STDIN), [
    'cafile' => $paths['ca']['cert'],
    'peer_name' => 'localhost',
    'verify_peer' => true,
    'verify_peer_name' => true,
    'local_cert' => $paths['client']['cert'],
    'local_pk' => $paths['client']['key'],
]);

// 13d. server checks multiple fingerprints all supported matched
$client = clientWithConfig(fgets(STDIN), [
    'cafile' => $paths['ca']['cert'],
    'peer_name' => 'localhost',
    'verify_peer' => true,
    'verify_peer_name' => true,
    'local_cert' => $paths['client']['cert'],
    'local_pk' => $paths['client']['key'],
]);
recvFirst($client);

// 13e. client checks multiple fingerprints matched
$client = clientWithConfig(fgets(STDIN), [
    'cafile' => $paths['ca']['cert'],
    'peer_name' => 'localhost',
    'verify_peer' => true,
    'verify_peer_name' => true,
    'local_cert' => $paths['client']['cert'],
    'local_pk' => $paths['client']['key'],
    'peer_fingerprint' => [
        'sha1' => $serverFingerprints['sha1'],
        'sha256' => $serverFingerprints['sha256'],
    ],
]);
sendFirst($client);

// 13f. client checks multiple fingerprints not matched
Assert::throws(static function () use ($paths, $badServerFingerprints): void {
    clientWithConfig(fgets(STDIN), [
        'cafile' => $paths['ca']['cert'],
        'peer_name' => 'localhost',
        'verify_peer' => true,
        'verify_peer_name' => true,
        'local_cert' => $paths['client']['cert'],
        'local_pk' => $paths['client']['key'],
        'peer_fingerprint' => [
            'sha1' => $badServerFingerprints['sha1'],
            'sha256' => $badServerFingerprints['sha256'],
        ],
    ]);
}, 'PHPWarningException', expectMessage: '/peer_fingerprint match failure|certificate verify failed/'); // the error message differs

// 13g. client checks multiple fingerprints partially matched
Assert::throws(static function () use ($paths, $serverFingerprints, $badServerFingerprints): void {
    clientWithConfig(fgets(STDIN), [
        'cafile' => $paths['ca']['cert'],
        'peer_name' => 'localhost',
        'verify_peer' => true,
        'verify_peer_name' => true,
        'local_cert' => $paths['client']['cert'],
        'local_pk' => $paths['client']['key'],
        'peer_fingerprint' => [
            'sha1' => $serverFingerprints['sha1'],
            'sha256' => $badServerFingerprints['sha256'],
        ],
    ]);
}, 'PHPWarningException', expectMessage: '/peer_fingerprint match failure|certificate verify failed/'); // the error message differs

// 13h. client checks multiple fingerprints all supported matched
$client = clientWithConfig(fgets(STDIN), [
    'cafile' => $paths['ca']['cert'],
    'peer_name' => 'localhost',
    'verify_peer' => true,
    'verify_peer_name' => true,
    'local_cert' => $paths['client']['cert'],
    'local_pk' => $paths['client']['key'],
    'peer_fingerprint' => $serverFingerprints,
]);
recvFirst($client);

printf('%s', fgets(STDIN));
printf("client end\n");
