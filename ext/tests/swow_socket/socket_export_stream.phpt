--TEST--
swow_socket: socket_export_stream hooking
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_extension_not_exist('sockets');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Socket;

$random = random_bytes(TEST_MAX_LENGTH);
if (PHP_OS_FAMILY !== 'Windows') {
    define('SERVER_SOCK1', '/tmp/swow_server_' . getRandomBytes(8) . '.sock');
    define('SERVER_SOCK2', '/tmp/swow_server_' . getRandomBytes(8) . '.sock');
    define('SERVER_SOCK3', '/tmp/swow_server_' . getRandomBytes(8) . '.sock');
    define('CLIENT_SOCK', '/tmp/swow_client_' . getRandomBytes(8) . '.sock');
} else {
    define('SERVER_SOCK1', '\\\\?\\pipe\\swow_server_' . getRandomBytes(8));
    define('SERVER_SOCK2', '\\\\?\\pipe\\swow_server_' . getRandomBytes(8));
    define('SERVER_SOCK3', '\\\\?\\pipe\\swow_server_' . getRandomBytes(8));
    define('CLIENT_SOCK', '\\\\?\\pipe\\swow_client_' . getRandomBytes(8));
}

function test($type, $server, $msg)
{
    switch ($type) {
        case 'tcp':
            $sock = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
            break;
        case 'udp':
            $sock = socket_create(AF_INET, SOCK_DGRAM, SOL_UDP);
            break;
        case 'unix':
        case 'pipe':
            $sock = socket_create(AF_UNIX, SOCK_STREAM, 0);
            break;
        case 'udg':
            $sock = socket_create(AF_UNIX, SOCK_DGRAM, 0);
            break;
        default:
            throw new Exception('not supported protocol');
    }
    Assert::notSame($sock, false);
    if ('unix' === $type || 'udg' === $type || 'pipe' === $type) {
        Assert::true(socket_bind($sock, CLIENT_SOCK));
        Assert::true(socket_connect($sock, $server->getSockAddress()));
    } else {
        Assert::true(socket_connect($sock, $server->getSockAddress(), $server->getSockPort()));
    }
    $stream = socket_export_stream($sock);
    Assert::notSame($stream, false);
    Assert::same(fwrite($stream, $msg), strlen($msg));
    sleep(0);
    Assert::same(fread($stream, strlen($msg)), $msg);
    socket_close($sock);
    if ('unix' === $type || 'udg' === $type || 'pipe' === $type) {
        ASSERT::true(unlink(CLIENT_SOCK));
    }
    echo "{$type} done\n";
}

// TCP test
$server = new Socket(Socket::TYPE_TCP);
Coroutine::run(function () use ($server) {
    $server->bind('127.0.0.1')->listen();
    $connection = $server->accept();
    $red = $connection->recvString(TEST_MAX_LENGTH);
    $connection->sendString($red);
    $connection->close();
    $server->close();
});
test('tcp', $server, $random);

// UDP test
$server = new Socket(Socket::TYPE_UDP);
Coroutine::run(function () use ($server, $random) {
    $server->bind('127.0.0.1');
    $red = $server->recvStringFrom(TEST_MAX_LENGTH, $addr, $port);
    //var_dump($red, $addr, $port);
    $server->sendStringTo($red, $addr, $port);
    $server->close();
});
test('udp', $server, $random);

if (PHP_OS_FAMILY != 'Windows') {
    // UNIX test
    $server = new Socket(Socket::TYPE_UNIX);
    Coroutine::run(function () use ($server, $random) {
        $server->bind(SERVER_SOCK1)->listen();
        $connection = $server->accept();
        $red = $connection->recvString(TEST_MAX_LENGTH, 0);
        $connection->sendString($red);
        $connection->close();
        $server->close();
    });
    test('unix', $server, $random);

    // PIPE test
    $server = new Socket(Socket::TYPE_PIPE);
    Coroutine::run(function () use ($server, $random) {
        $server->bind(SERVER_SOCK3)->listen();
        $connection = $server->accept();
        $red = $connection->recvString(TEST_MAX_LENGTH, 0);
        $connection->sendString($red);
        $connection->close();
        $server->close();
    });
    test('pipe', $server, $random);

    // UDG test
    $server = new Socket(Socket::TYPE_UDG);
    Coroutine::run(function () use ($server, $random) {
        $server->bind(SERVER_SOCK2);
        $red = $server->recvStringFrom(TEST_MAX_LENGTH, $addr);
        $server->sendStringTo($red, $addr);
        $server->close();
    });
    test('udg', $server, $random);
} else {
    echo 'unix skip' . PHP_EOL;
    echo 'pipe skip' . PHP_EOL;
    echo 'udg skip' . PHP_EOL;
}

?>
--EXPECTREGEX--
tcp done
udp done
unix (done|skip)
pipe (done|skip)
udg (done|skip)
