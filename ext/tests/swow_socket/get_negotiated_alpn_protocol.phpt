--TEST--
swow_socket: getNegotiatedAlpnProtocol
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!Swow\Extension::isBuiltWith('openssl'), 'extension must be built with ssl');
skip_if(!Swow\Extension::isBuiltWith('libcurl'), 'extension must be built with libcurl');
skip_if(!defined('CURL_HTTP_VERSION_2_0'), 'curl must be built with HTTP/2 support');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Socket;
use Swow\Sync\WaitReference;

$paths = testX509Paths(__DIR__ . '/getNegotiatedAlpnProtocol');

$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1', 0)->listen();
$port = $server->getSockPort();
$wr = new WaitReference();

Coroutine::run(static function () use ($server, $paths, $wr): void {
    $connection = $server->accept()->enableCrypto([
        'certificate' => $paths['localhost']['cert'],
        'certificate_key' => $paths['localhost']['key'],
        'verify_peer' => false,
        'alpn_protocols' => 'h2,http/1.1',
    ]);

    echo 'server:' . ($connection->getNegotiatedAlpnProtocol() ?? 'null') . PHP_EOL;
    $connection->close();
});

$client = (new Socket(Socket::TYPE_TCP))->connect('127.0.0.1', $port);
$client->enableCrypto([
    'ca_file' => $paths['ca']['cert'],
    'verify_peer' => true,
    'peer_name' => 'localhost',
    'alpn_protocols' => 'h2',
]);
echo 'client:' . ($client->getNegotiatedAlpnProtocol() ?? 'null') . PHP_EOL;
$client->close();

$wr::wait($wr);
$server->close();
?>
--EXPECT--
client:h2
server:h2
