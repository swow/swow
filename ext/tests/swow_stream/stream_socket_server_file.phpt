--TEST--
swow_stream: stream_socket_server with file:// stream
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!extension_loaded('openssl'), 'openssl extension is required');
skip_if(!Swow\Extension::isBuiltWith('openssl'), 'extension must be built with ssl');
?>
--FILE--
<?php
require_once __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Sync\WaitGroup;

$certFile = __DIR__ . DIRECTORY_SEPARATOR . 'stream_socket_server.pem.tmp';
$cacertFile = __DIR__ . DIRECTORY_SEPARATOR . 'stream_socket_server-ca.pem.tmp';

$certificateGenerator = new CertificateGenerator();
$certificateGenerator->saveCaCert($cacertFile);
$certificateGenerator->saveNewCertAsFileWithKey('stream_socket_server', $certFile);

$wg = new WaitGroup();
$wg->add();

Coroutine::run(static function () use ($wg, $certFile): void {
    $serverUri = 'ssl://0.0.0.0:12346';
    $serverFlags = STREAM_SERVER_BIND | STREAM_SERVER_LISTEN;
    $serverCtx = stream_context_create(['ssl' => [
        'local_cert' => $certFile,
    ]]);

    $server = stream_socket_server($serverUri, $errno, $errMessage, $serverFlags, $serverCtx);
    $wg->done();

    $ret = stream_socket_accept($server);
    fread($ret, 4096); // wait for client to send request
    fwrite($ret, "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 12\r\nConnection: close\r\n\r\nHello World!");
    fclose($ret);
    sleep(1);
});

$wg->wait();
$context = stream_context_create(['ssl' => [
    'cafile' => "file://{$cacertFile}",
    'peer_name' => 'stream_socket_server',
]]);
$ret = file_get_contents('https://localhost:12346', context: $context);
var_dump($ret);

echo 'Done' . PHP_EOL;

?>
--CLEAN--
<?php
@unlink(__DIR__ . DIRECTORY_SEPARATOR . 'stream_socket_server.pem.tmp');
@unlink(__DIR__ . DIRECTORY_SEPARATOR . 'stream_socket_server-ca.pem.tmp');
?>
--EXPECT--
string(12) "Hello World!"
Done
