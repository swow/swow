--TEST--
swow_socket: tls 256k truncate bug
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

foreach ([1, 2048, 4096, 16384, 32768] as $tailLength) {
    var_dump($tailLength);
    $tail = str_repeat('b', $tailLength);
    $data = str_repeat('a', 256 * 1024) . $tail;

    // start tls server
    $paths = testX509Paths(__DIR__ . '/tls_truncate_256kX509');

    $server = new Socket(Socket::TYPE_TCP);
    $server->bind('127.0.0.1', 0);
    // var_dump($server->getSockPort());

    Coroutine::run(static function () use ($server, $paths, $data): void {
        $server->listen();
        $conn = $server->accept()->enableCrypto([
            'ca_file' => $paths['ca']['cert'],
            'certificate' => $paths['localhost']['cert'],
            'certificate_key' => $paths['localhost']['key'],
            'verify_peer' => false,
            'verify_peer_name' => false,
        ]);
        $read = $conn->readString(strlen($data));
        // send tail
        $conn->send(substr($read, 256 * 1024));
        $conn->close();
    });

    $client = new Socket(Socket::TYPE_TCP);
    $client->connect('127.0.0.1', $server->getSockPort());
    $client->enableCrypto([
        'ca_file' => $paths['ca']['cert'],
        'peer_name' => 'localhost',
        'verify_peer' => true,
        'verify_peer_name' => true,
    ]);
    $client->send($data);
    $received = $client->readString(strlen($tail));

    Assert::same($tail, $received, 'Received data does not match sent data with tail length ' . $tailLength);
}

echo "Done\n";
?>
--CLEAN--
<?php
require __DIR__ . '/../include/bootstrap.php';

@rmtree(__DIR__ . '/tls_truncate_256kX509');
?>
--EXPECT--
int(1)
int(2048)
int(4096)
int(16384)
int(32768)
Done
