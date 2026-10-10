--TEST--
swow_curl: large HEAD content lengths retain their integer width
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
skip_if(PHP_INT_SIZE < 8, 'the declared content length requires 64-bit PHP integers');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Socket;

$length = (1 << 32) + 17;
$server = new Socket(Socket::TYPE_TCP);
$server->setTimeout(1000)->bind('127.0.0.1')->listen();
$request = '';
$peer = Coroutine::run(static function () use ($server, $length, &$request): void {
    $connection = $server->accept(1000);
    try {
        do {
            $request .= $connection->recvString();
        } while (!str_contains($request, "\r\n\r\n"));
        // A HEAD response describes the representation size without transferring its body.
        $connection->send("HTTP/1.1 200 OK\r\nContent-Length: {$length}\r\nConnection: close\r\n\r\n");
    } finally {
        $connection->close();
    }
});
$handle = curl_init('http://' . $server->getSockAddress() . ':' . $server->getSockPort() . '/metadata');
try {
    Assert::true(curl_setopt_array($handle, [
        CURLOPT_PROXY => '',
        CURLOPT_NOBODY => true,
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_TIMEOUT_MS => 1000,
    ]));
    Assert::same(curl_exec($handle), '');
    Assert::startsWith($request, 'HEAD /metadata HTTP/1.1');
    Assert::same(curl_getinfo($handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T), $length);
    Assert::same(curl_getinfo($handle)['download_content_length'], (float) $length);
    Assert::same(curl_getinfo($handle, CURLINFO_FILETIME), -1);
} finally {
    unset($handle);
    $server->close();
    if ($peer->isAlive()) {
        $peer->kill();
    }
}
echo "Done\n";
?>
--EXPECT--
Done
