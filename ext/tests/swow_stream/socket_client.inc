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

printf('%s', fgets(STDIN));
printf("client end\n");
