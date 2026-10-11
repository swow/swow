--TEST--
swow_curl: HTTP/2 GOAWAY completes accepted streams and reconnects subsequent requests
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
        $previous = null;
        for ($cycle = 0; $cycle < 8; $cycle++) {
            $transfer = new Http2Transfer($server, '/goaway');
            $transfer->add($multi);
            Http2Transfer::assertSuccessful(Http2Transfer::run($multi), 1);
            $closed = $transfer->response();
            if ($previous !== null) {
                Assert::same($closed['session'], $previous['session']);
            }
            $transfer->remove($multi);
            $transfer = new Http2Transfer($server, '/after-goaway');
            $transfer->add($multi);
            Http2Transfer::assertSuccessful(Http2Transfer::run($multi), 1);
            $previous = $transfer->response();
            Assert::greaterThan($previous['session'], $closed['session']);
            Assert::same($previous['stream'], 1);
            Assert::same($previous['path'], '/after-goaway');
            Assert::same(curl_getinfo($transfer->handle, CURLINFO_NUM_CONNECTS), 1);
            $transfer->remove($multi);
        }
    } finally {
        unset($transfer, $multi);
        gc_collect_cycles();
        $server->stop();
    }
}
echo "Done\n";
?>
--EXPECT--
Done
