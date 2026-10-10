--TEST--
swow_curl: cookie sharing and share-handle ownership across transfers
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Socket;

$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1')->listen();
$coroutine = null;
$first = $second = $share = null;
try {
    $share = curl_share_init();
    Assert::true(curl_share_setopt($share, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE));
    Assert::same(curl_share_errno($share), 0);
    Assert::same(curl_share_strerror(curl_share_errno($share)), 'No error');
    Assert::throws(static function () use ($share): void {
        curl_share_setopt($share, -1, 0);
    }, ValueError::class, expectMessage: '/valid cURL share option/');
    Assert::greaterThan(curl_share_errno($share), 0);
    Assert::true(curl_share_setopt($share, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE));

    $first = curl_init();
    $second = curl_init('http://' . $server->getSockAddress() . ':' . $server->getSockPort() . '/cookies');
    foreach ([$first, $second] as $handle) {
        Assert::true(curl_setopt_array($handle, [
            CURLOPT_SHARE => $share,
            CURLOPT_COOKIEFILE => '',
            CURLOPT_PROXY => '',
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_TIMEOUT_MS => 1000,
        ]));
    }
    unset($handle);
    $cookie = "127.0.0.1\tFALSE\t/\tFALSE\t0\tshared\tyes";
    Assert::true(curl_setopt($first, CURLOPT_COOKIELIST, $cookie));
    Assert::same(curl_getinfo($second, CURLINFO_COOKIELIST), [$cookie]);
    // Transfer handles must retain the shared cookie store after these references disappear.
    unset($first, $share);
    $coroutine = Coroutine::run(static function () use ($server): void {
        $connection = $server->accept(1000);
        $connection->setTimeout(1000);
        try {
            $request = '';
            while (!str_contains($request, "\r\n\r\n")) {
                $request .= $connection->recvString();
            }
            Assert::contains($request, "\r\nCookie: shared=yes\r\n");
            $connection->send("HTTP/1.1 200 OK\r\nContent-Length: 4\r\nConnection: close\r\n\r\nDone");
        } finally {
            $connection->close();
        }
    });
    Assert::same(curl_exec($second), 'Done');
    Assert::false($coroutine->isAlive());
} finally {
    unset($first, $second, $share);
    $server->close();
    if ($coroutine?->isAlive()) {
        $coroutine->kill();
    }
}

echo "Done\n";
?>
--EXPECT--
Done
