--TEST--
swow_stream: fsockopen
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!Swow\Extension::isBuiltWith('openssl'), 'extension must be built with ssl');
?>
--CLEAN--
<?php
@unlink(__DIR__ . '/fsockopen.sock');
?>
--INI--
openssl.cafile={PWD}/fsockopen/ca.crt
--FILE--
<?php
require_once __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Socket;

// prepare tls certs
$paths = testX509Paths(__DIR__ . '/fsockopen');
[$tcpServer, $tcpCoroutine] = createHttpTestServer();
[$tlsServer, $tlsCoroutine] = createHttpTestServer($paths['localhost']['cert'], $paths['localhost']['key']);

// test tcp stream with fsockopen
$stream = fsockopen('tcp://127.0.0.1', $tcpServer->getSockPort(), $errno, $errstr, 10);
Assert::notSame($stream, false);
Assert::notSame(fwrite($stream, "GET / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"), false);
Assert::contains(stream_get_contents($stream), 'Done');
fclose($stream);

// test tls stream with fsockopen
$stream = fsockopen('tls://localhost', $tlsServer->getSockPort(), $errno, $errstr, 10);
Assert::notSame($stream, false);
Assert::notSame(fwrite($stream, "GET / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"), false);
Assert::contains(stream_get_contents($stream), 'Done');
fclose($stream);

if (in_array('unix', stream_get_transports(), true)) {
    // test unix stream with fsockopen

    $server = new Socket(Socket::TYPE_UNIX);
    $server->bind('./fsockopen.sock');
    $server->listen();
    $unixCoroutine = Coroutine::run(static function () use ($server): void {
        $connection = $server->accept();
        $connection->send('Done');
        $connection->close();
        $server->close();
    });

    $stream = fsockopen('unix://./fsockopen.sock', -1, $errno, $errstr, 10000);
    Assert::notSame($stream, false);
    Assert::same(fread($stream, 1024), 'Done');
    fclose($stream);
}

$tcpServer->close();
$tlsServer->close();

echo "Done\n";
?>
--EXPECT--
Done
