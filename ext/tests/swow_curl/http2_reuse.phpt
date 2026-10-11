--TEST--
swow_curl: HTTP/2 easy and multi reuse retain connections across stream removal
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
require __DIR__ . '/../include/autoload.php';
Http2Server::skipIfUnavailable();
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

foreach ([false, true] as $tls) {
    $server = new Http2Server($tls);
    $multi = Http2Transfer::multi();
    try {
        $first = null;
        $previousStream = 0;
        $transfer = new Http2Transfer($server, '/reuse');
        for ($index = 0; $index < 16; $index++) {
            // Reuse the same easy first, then replace it while retaining the multi connection cache.
            if ($index >= 8) {
                $transfer = new Http2Transfer($server, '/reuse');
            }
            $transfer->clear();
            $transfer->add($multi);
            Http2Transfer::assertSuccessful(Http2Transfer::run($multi), 1);
            $response = $transfer->response();
            $first ??= $response;
            Assert::same($response['session'], $first['session']);
            Assert::same($response['port'], $first['port']);
            Assert::same($response['alpn'], $tls ? 'h2' : 'h2c');
            Assert::greaterThan($response['stream'], $previousStream);
            Assert::same($response['path'], '/reuse');
            Assert::same(curl_getinfo($transfer->handle, CURLINFO_NUM_CONNECTS), $index === 0 ? 1 : 0);
            $previousStream = $response['stream'];
            $transfer->remove($multi);
        }
        unset($transfer);
        $easy = new Http2Transfer($server, '/easy');
        $first = null;
        $previousStream = 0;
        for ($index = 0; $index < 8; $index++) {
            $easy->clear();
            Assert::true(curl_exec($easy->handle));
            Assert::same(curl_errno($easy->handle), CURLE_OK);
            Assert::same(curl_getinfo($easy->handle, CURLINFO_HTTP_VERSION), CURL_HTTP_VERSION_2_0);
            $response = $easy->response();
            $first ??= $response;
            Assert::same($response['session'], $first['session']);
            Assert::same($response['port'], $first['port']);
            Assert::greaterThan($response['stream'], $previousStream);
            $previousStream = $response['stream'];
        }
    } finally {
        unset($easy, $transfer, $multi);
        gc_collect_cycles();
        $server->stop();
    }
}
echo "Done\n";
?>
--EXPECT--
Done
