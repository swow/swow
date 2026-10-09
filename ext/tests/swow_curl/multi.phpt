--TEST--
swow_curl: multi
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_SAPI !== 'cli', 'only for cli');
skip_if(!Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Http\Status;
use Swow\Socket;
use Swow\Sync\WaitReference;

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

$wr = new WaitReference();
Coroutine::run(static function () use ($server, $wr): void {
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
    echo "Multi select done\n";

    foreach ($chs as $ch) {
        Assert::eq(curl_getinfo($ch, CURLINFO_HTTP_CODE), Status::OK);
        $response = curl_multi_getcontent($ch);
        Assert::eq($response, 'OK');
        curl_multi_remove_handle($mh, $ch);
        if (PHP_VERSION_ID < 80100) {
            curl_close($ch);
        }
    }
});

echo "Multi is running\n";

$wr::wait($wr);

echo "Done\n";
?>
--EXPECT--
Multi is running
Multi select done
Done
