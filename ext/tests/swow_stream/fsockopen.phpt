--TEST--
swow_stream: fsockopen
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_offline();
?>
--CLEAN--
<?php
@unlink(__DIR__ . '/fsockopen.sock');
?>
--FILE--
<?php
require_once __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Socket;

// prepare tls certs
$paths = testX509Paths(__DIR__ . '/fsockopen');

// test tcp stream with fsockopen
$stream = fsockopen('tcp://www.apple.com', 80, $errno, $errstr, 10000);
Assert::notSame($stream, false);
Assert::notSame(fwrite($stream, "GET / HTTP/1.1\r\nHost: www.apple.com\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"), false);
Assert::contains(stream_get_contents($stream), 'apple');
fclose($stream);

// test tls stream with fsockopen
$stream = fsockopen('tls://www.apple.com', 443, $errno, $errstr, 10000);
Assert::notSame($stream, false);
Assert::notSame(fwrite($stream, "GET / HTTP/1.1\r\nHost: www.apple.com\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"), false);
Assert::contains(stream_get_contents($stream), 'apple');
fclose($stream);

if (in_array('unix', stream_get_transports(), true)) {
    // test unix stream with fsockopen

    $server = new Socket(Socket::TYPE_UNIX);
    $server->bind('./fsockopen.sock');
    $server->listen();
    Coroutine::run(static function () use ($server): void {
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

echo "Done\n";
?>
--EXPECT--
Done
