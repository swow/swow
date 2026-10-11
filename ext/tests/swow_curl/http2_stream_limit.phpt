--TEST--
swow_curl: HTTP/2 queues transfers beyond the peer stream limit on one connection
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
    $server = new Http2Server($tls, maxStreams: 2);
    $multi = Http2Transfer::multi();
    $transfers = [];
    try {
        // Receive SETTINGS before opening the batch; the initial protocol default permits more streams.
        $warmup = new Http2Transfer($server, '/settings');
        $warmup->add($multi);
        Http2Transfer::assertSuccessful(Http2Transfer::run($multi), 1);
        $first = $warmup->response();
        $warmup->remove($multi);
        // Each pair releases the next queued pair; a single admitted stream cannot pass the barrier.
        for ($index = 0; $index < 12; $index++) {
            $transfer = new Http2Transfer($server, "/barrier?group=limit&count=2&id={$index}", 100);
            $transfer->add($multi);
            $transfers[] = $transfer;
        }
        Http2Transfer::assertSuccessful(Http2Transfer::run($multi), 12);
        $streams = [];
        foreach ($transfers as $index => $transfer) {
            $response = $transfer->response();
            Assert::same($response['session'], $first['session']);
            Assert::same($response['batch'], 2);
            Assert::same($response['path'], "/barrier?group=limit&count=2&id={$index}");
            $streams[] = $response['stream'];
            $transfer->remove($multi);
        }
        Assert::same(count(array_unique($streams)), 12);
    } finally {
        unset($warmup, $transfer, $transfers, $multi);
        gc_collect_cycles();
        $server->stop();
    }
}
echo "Done\n";
?>
--EXPECT--
Done
