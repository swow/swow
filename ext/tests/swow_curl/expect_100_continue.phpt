--TEST--
swow_curl: the libcurl timer reaches the event loop while waiting for 100-continue
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
?>
--FILE--
<?php

require_once __DIR__ . '/../include/bootstrap.php';

use Swow\Buffer;
use Swow\Coroutine;
use Swow\Socket;

/*
 * libcurl waits for "100 Continue" only until CURLOPT_EXPECT_100_TIMEOUT_MS
 * expires, and that deadline is delivered through CURLMOPT_TIMERFUNCTION, not
 * through socket activity. Swow replaces the blocking curl_easy_perform() with
 * its own event loop, so a timer that never reaches the loop leaves the client
 * waiting for a response that will not come.
 *
 * The server below withholds "100 Continue" and counts the bytes it receives,
 * so the response carries the verdict: a client whose timer works gives up
 * after the timeout and sends the body anyway, a client whose timer is lost
 * sends nothing.
 */
$bodyLength = 1048576;
$server = (new Socket(Socket::TYPE_TCP))
    ->bind('127.0.0.1', 0)->listen();
Coroutine::run(static function () use ($server, $bodyLength): void {
    $connection = $server->accept();
    $buffer = new Buffer(8192);
    // read the request headers and not a byte more: the body is the answer
    $headers = '';
    while (!str_contains($headers, "\r\n\r\n")) {
        $buffer->truncate(0);
        if ($connection->recv($buffer, timeout: 5) <= 0) {
            break;
        }
        $headers .= $buffer->toString();
    }
    $received = 0;
    while ($received < $bodyLength) {
        $buffer->truncate(0);
        try {
            $length = $connection->recv($buffer, timeout: 3000);
        } catch (Throwable) {
            break;
        }
        if ($length <= 0) {
            break;
        }
        $received += $length;
    }
    $verdict = $received >= $bodyLength ? 'body-sent' : "body-missing:{$received}";
    try {
        $connection->send(
            "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " .
            strlen($verdict) . "\r\n\r\n" . $verdict
        );
    } catch (Throwable) {
        // the client gave up already, nothing to report to it
    }
    $connection->close();
    $server->close();
});

$uploadFile = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'swow_test_curl_expect_100_upload';
$downloadFile = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'swow_test_curl_expect_100_download';

file_put_contents($uploadFile, str_repeat('?', $bodyLength));
$download = fopen($downloadFile, 'wb');

$ch = curl_init('127.0.0.1:' . $server->getSockPort() . '/upload?arg=1');
curl_setopt_array($ch, [
    CURLOPT_POST => true,
    CURLOPT_POSTFIELDS => ['file' => curl_file_create($uploadFile)],
    CURLOPT_FILE => $download,
    CURLOPT_HTTPHEADER => ['accept: text/plain'],
    // libcurl's default is 1000 ms, which is also how long the peer would have
    // to answer; the point here is that the timer is delivered at all
    CURLOPT_EXPECT_100_TIMEOUT_MS => 100,
    CURLOPT_TIMEOUT_MS => 10000,
]);

Assert::true(curl_exec($ch), curl_error($ch));
fclose($download);
Assert::same(file_get_contents($downloadFile), 'body-sent');

echo "Done\n";

?>
--CLEAN--
<?php
@unlink(sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'swow_test_curl_expect_100_upload');
@unlink(sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'swow_test_curl_expect_100_download');
?>
--EXPECT--
Done
