--TEST--
swow_socket: tls 256k truncate bug
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Socket;

// prepare data longer than 256k
$data = "";
$seed = 42;
while (strlen($data) < 256 * 1024 + 1) {
    // simple LCG with fixed seed
    $seed = (75 * $seed + 74) % 65537;
    $data .= (string)$seed;
}
$tail = substr($data, 256 * 1024);
// var_dump($tail);

// start tls server
$paths = testX509Paths(__DIR__ . '/tls_truncate_256kX509');

$server = new Socket(Socket::TYPE_TCP);
$server->bind("127.0.0.1", 0);
// var_dump($server->getSockPort());

Coroutine::run(function () use ($server, $paths, $data) {
    $server->listen();
    $conn = $server->accept()->enableCrypto([
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
$client->connect("127.0.0.1", $server->getSockPort());
$client->enableCrypto([
    'ca_file' => $paths['ca']['cert'],
    'peer_name' => 'localhost',
    'verify_peer' => true,
    'verify_peer_name' => true,
]);
$client->send($data);
$received = $client->readString(strlen($tail));

Assert::same($tail, $received, 'Received data does not match sent data');

echo "Done\n";
?>
--CLEAN--
<?php
require __DIR__ . '/../include/bootstrap.php';

@rmtree(__DIR__ . '/tls_truncate_256kX509');
?>
--EXPECT--
Done
