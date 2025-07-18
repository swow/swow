<?php
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
    $server = stream_socket_server('ssl://127.0.0.1:0', $errno, $errstr, STREAM_SERVER_BIND | STREAM_SERVER_LISTEN, $context);
    return $server;
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

printf("server end\n");
