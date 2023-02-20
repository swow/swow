--TEST--
swow_curl: multi
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_extension_not_exist('curl');
skip_if(PHP_SAPI !== 'cli', 'only for cli');
skip_if(!getenv('SWOW_HAVE_CURL') && !Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Http\Status;
use Swow\Socket;

$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1', 0)->listen();
Coroutine::run(static function () use ($server): void {
    while (true) {
        $connection = $server->accept();
        Coroutine::run(static function () use ($connection): void {
            $connection->recvString();
            $connection->send("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 2\r\n\r\nOK");
        });
    }
});

// create the multiple cURL handle
$mh = curl_multi_init();

$chs = [];
for ($n = 0; $n < TEST_MAX_CONCURRENCY; $n++) {
    $chs[] = $ch = curl_init();
    curl_setopt($ch, CURLOPT_URL, "http://{$server->getSockAddress()}:{$server->getSockPort()}/");
    curl_setopt($ch, CURLOPT_HEADER, 0);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
    curl_multi_add_handle($mh, $ch);
}

do {
    $status = curl_multi_exec($mh, $active);
    if ($active) {
        $r = curl_multi_select($mh);
        Assert::greaterThanEq($r, 0);
    }
} while ($active && $status === CURLM_OK);

foreach ($chs as $ch) {
    Assert::eq(curl_getinfo($ch, CURLINFO_HTTP_CODE), Status::OK);
    $response = curl_multi_getcontent($ch);
    Assert::eq($response, 'OK');
    curl_multi_remove_handle($mh, $ch);
    curl_close($ch);
}

echo "Done\n";
?>
--EXPECT--
Done
