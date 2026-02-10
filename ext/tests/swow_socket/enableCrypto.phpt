--TEST--
swow_socket: enableCrypto
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!Swow\Extension::isBuiltWith('openssl'), 'extension must be built with ssl');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Socket;

// prepare tls certs
$paths = testX509Paths(__DIR__ . '/enableCryptoX509');

$server = new Socket(Socket::TYPE_TCP);
$server->bind('localhost', 0);
$port = $server->getSockPort();

function sendFirst(Socket $conn): void
{
    static $callTime = 0;
    $callTime++;
    $conn->send("sendFirst{$callTime}");
    $data = $conn->recvString(1024);
    Assert::same("recvFirst{$callTime}", $data);
    $conn->close();
}

function recvFirst(Socket $conn): void
{
    static $callTime = 0;
    $callTime++;
    $data = $conn->recvString(1024);
    Assert::same("sendFirst{$callTime}", $data);
    $conn->send("recvFirst{$callTime}");
    $conn->close();
}

Coroutine::run(static function () use ($server, $paths): void {
    $server->listen();

    // 1. enableCrypto with empty arguments (should fail)
    Assert::throws(static function () use ($server): void {
        $server->accept()->enableCrypto();
    }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed.+/');

    // 2. enableCrypto with empty array (should fail)
    Assert::throws(static function () use ($server): void {
        $server->accept()->enableCrypto([]);
    }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed.+/');

    // 3a. enableCrypto server with valid certificate, client not specified ca_file
    Assert::throws(static function () use ($server, $paths): void {
        $server->accept()->enableCrypto([
            'certificate' => $paths['localhost']['cert'],
            'certificate_key' => $paths['localhost']['key'],
        ]);
    }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed.+/');

    // 3b. enableCrypto server with valid certificate, client donot accept the ca
    Assert::throws(static function () use ($server, $paths): void {
        $server->accept()->enableCrypto([
            'certificate' => $paths['localhost']['cert'],
            'certificate_key' => $paths['localhost']['key'],
        ]);
    }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed.+/');

    // 3c. enableCrypto server with valid certificate, client accept the ca
    $conn = $server->accept()->enableCrypto([
        'certificate' => $paths['localhost']['cert'],
        'certificate_key' => $paths['localhost']['key'],
        'verify_peer' => false,
    ]);
    recvFirst($conn);

    // 4a. server checks client certificate, client have no certificate
    Assert::throws(static function () use ($server, $paths): void {
        $server->accept()->enableCrypto([
            'ca_file' => $paths['ca']['cert'],
            'certificate' => $paths['localhost']['cert'],
            'certificate_key' => $paths['localhost']['key'],
            'peer_name' => 'client',
            'verify_peer' => true,
            'verify_peer_name' => true,
        ]);
    }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed.+/');

    // 4b. server checks client certificate, client have certificate but not match peer_name
    Assert::throws(static function () use ($server, $paths): void {
        $server->accept()->enableCrypto([
            'ca_file' => $paths['ca']['cert'],
            'certificate' => $paths['localhost']['cert'],
            'certificate_key' => $paths['localhost']['key'],
            'peer_name' => 'client2',
            'verify_peer' => true,
            'verify_peer_name' => true,
        ]);
    }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed.+/');

    // 4c. server checks client certificate, client have valid certificate
    $conn = $server->accept()->enableCrypto([
        'ca_file' => $paths['ca']['cert'],
        'certificate' => $paths['localhost']['cert'],
        'certificate_key' => $paths['localhost']['key'],
        'peer_name' => 'client.local',
        'verify_peer' => true,
        'verify_peer_name' => true,
    ]);
    sendFirst($conn);

    // 5. server with valid certificate, donot check client certificate
    $conn = $server->accept()->enableCrypto([
        'certificate' => $paths['localhost']['cert'],
        'certificate_key' => $paths['localhost']['key'],
        'verify_peer' => false,
    ]);
    recvFirst($conn);

    // 6a. enableCrypto server with valid certificate, bad peer name
    Assert::throws(static function () use ($server, $paths): void {
        $server->accept()->enableCrypto([
            'certificate' => $paths['localhost']['cert'],
            'certificate_key' => $paths['localhost']['key'],
            'verify_peer' => false,
        ]);
    }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed.+/');

    // 6b. enableCrypto server with valid certificate, bad peer name, but client accept it
    $conn = $server->accept()->enableCrypto([
        'certificate' => $paths['localhost']['cert'],
        'certificate_key' => $paths['localhost']['key'],
        'verify_peer' => false,
    ]);
    sendFirst($conn);

    // 7a. enableCrypto server with very deep certificate chain, client reject it
    Assert::throws(static function () use ($server, $paths): void {
        $server->accept()->enableCrypto([
            'ca_file' => $paths['ca']['cert'],
            'certificate' => $paths['verydeep']['cert'],
            'certificate_key' => $paths['verydeep']['key'],
            'verify_peer' => false,
        ]);
    }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed.+/');

    // 7b. enableCrypto server with very deep certificate chain, client accept it
    $conn = $server->accept()->enableCrypto([
        'ca_file' => $paths['ca']['cert'],
        'certificate' => $paths['verydeep']['cert'],
        'certificate_key' => $paths['verydeep']['key'],
        'verify_peer' => false,
    ]);
    recvFirst($conn);

    // 8a. self signed certificate
    Assert::throws(static function () use ($server, $paths): void {
        $server->accept()->enableCrypto([
            'certificate' => $paths['selfsigned']['cert'],
            'certificate_key' => $paths['selfsigned']['key'],
            'verify_peer' => false,
        ]);
    }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed.+/');

    // 8b. self signed certificate, but client accept it
    $conn = $server->accept()->enableCrypto([
        'certificate' => $paths['selfsigned']['cert'],
        'certificate_key' => $paths['selfsigned']['key'],
        'verify_peer' => false,
    ]);
    recvFirst($conn);
});

// 1. enableCrypto with empty arguments (should fail)
$client = (new Socket(Socket::TYPE_TCP))->connect('localhost', $port);
Assert::throws(static function () use ($client): void {
    $client->enableCrypto();
}, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed.+/');

// 2. enableCrypto with empty array (should fail)
$client = (new Socket(Socket::TYPE_TCP))->connect('localhost', $port);
Assert::throws(static function () use ($client): void {
    $client->enableCrypto([]);
}, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed.+/');

// 3a. enableCrypto server with valid certificate, client not specified ca_file
$client = (new Socket(Socket::TYPE_TCP))->connect('localhost', $port);
Assert::throws(static function () use ($client, $paths): void {
    $client->enableCrypto([
        'verify_peer' => true,
    ]);
}, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed.+/');

// 3b. enableCrypto server with valid certificate, client donot accept the ca
$client = (new Socket(Socket::TYPE_TCP))->connect('localhost', $port);
Assert::throws(static function () use ($client, $paths): void {
    $client->enableCrypto([
        'ca_file' => $paths['ca2']['cert'],
        'verify_peer' => true,
    ]);
}, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed.+/');

// 3c. enableCrypto server with valid certificate, client accept the ca
$client = (new Socket(Socket::TYPE_TCP))->connect('localhost', $port);
$client->enableCrypto([
    'ca_file' => $paths['ca']['cert'],
    'verify_peer' => true,
]);
sendFirst($client);

// 4a. server checks client certificate, client have no certificate
$client = (new Socket(Socket::TYPE_TCP))->connect('localhost', $port);
// this wont fail: server will refuse the client after serverHello
// at that time, client will think its handshake is finished
$client->enableCrypto([
    'ca_file' => $paths['ca']['cert'],
    'verify_peer' => true,
]);

// 4b. server checks client certificate, client have certificate but not match peer_name
$client = (new Socket(Socket::TYPE_TCP))->connect('localhost', $port);
Assert::throws(static function () use ($client, $paths): void {
    $client->enableCrypto([
        'ca_file' => $paths['client']['cert'],
        'verify_peer' => true,
    ]);
}, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed.+/');

// 4c. server checks client certificate, client have valid certificate
$client = (new Socket(Socket::TYPE_TCP))->connect('localhost', $port);
$client->enableCrypto([
    'ca_file' => $paths['ca']['cert'],
    'certificate' => $paths['client']['cert'],
    'certificate_key' => $paths['client']['key'],
    'peer_name' => 'localhost',
    'verify_peer' => true,
    'verify_peer_name' => true,
]);
recvFirst($client);

// 5. enableCrypto client with valid certificate
$client = (new Socket(Socket::TYPE_TCP))->connect('localhost', $port);
$client->enableCrypto([
    'ca_file' => $paths['ca']['cert'],
    'verify_peer' => true,
]);
sendFirst($client);

// 6a. enableCrypto server with valid certificate, bad peer name
$client = (new Socket(Socket::TYPE_TCP))->connect('localhost', $port);
Assert::throws(static function () use ($client, $paths): void {
    $client->enableCrypto([
        'ca_file' => $paths['ca']['cert'],
        'peer_name' => 'notlocalhost',
        'verify_peer' => true,
        'verify_peer_name' => true,
    ]);
}, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed.+/');

// 6b. enableCrypto server with valid certificate, bad peer name, but client accept it
$client = (new Socket(Socket::TYPE_TCP))->connect('localhost', $port);
$client->enableCrypto([
    'ca_file' => $paths['ca']['cert'],
    'verify_peer' => true,
    'verify_peer_name' => false,
]);
recvFirst($client);

// 7a. enableCrypto server with very deep certificate chain, client reject it
$client = (new Socket(Socket::TYPE_TCP))->connect('localhost', $port);
Assert::throws(static function () use ($client, $paths): void {
    $client->enableCrypto([
        'ca_file' => $paths['ca']['cert'],
        'verify_peer' => true,
    ]);
}, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed.+/');

// 7b. enableCrypto server with very deep certificate chain, client accept it
$client = (new Socket(Socket::TYPE_TCP))->connect('localhost', $port);
$client->enableCrypto([
    'ca_file' => $paths['ca']['cert'],
    'verify_peer' => true,
    'verify_depth' => 99,
]);
sendFirst($client);

// 8a. self signed certificate
$client = (new Socket(Socket::TYPE_TCP))->connect('localhost', $port);
Assert::throws(static function () use ($client, $paths): void {
    $client->enableCrypto();
}, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed.+/');

// 8b. self signed certificate, but client accept it
$client = (new Socket(Socket::TYPE_TCP))->connect('localhost', $port);
$client->enableCrypto([
    'allow_self_signed' => true,
]);
sendFirst($client);

echo 'Done' . PHP_EOL;
?>
--CLEAN--
<?php
require __DIR__ . '/../include/bootstrap.php';

@rmtree(__DIR__ . '/enableCryptoX509');
?>
--EXPECT--
Done
