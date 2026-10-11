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
    $streamLimit = 2;
    $server = new Http2Server($tls, maxStreams: $streamLimit);
    $multi = Http2Transfer::multi();
    $transfers = [];
    try {
        // Exercise the client option and peer limit together, with excess easy handles queued.
        if (defined('CURLMOPT_MAX_CONCURRENT_STREAMS')) {
            Assert::true(curl_multi_setopt($multi, CURLMOPT_MAX_CONCURRENT_STREAMS, $streamLimit));
        }
        $first = Http2Transfer::establish($server, $multi);
        // Each pair releases the next queued pair; a single admitted stream cannot pass the barrier.
        for ($index = 0; $index < 12; $index++) {
            $transfer = new Http2Transfer($server, "/barrier?group=limit&count={$streamLimit}&id={$index}", 100);
            $transfer->add($multi);
            $transfers[] = $transfer;
        }
        Http2Transfer::assertSuccessful(Http2Transfer::run($multi), 12);
        $streams = [];
        foreach ($transfers as $index => $transfer) {
            $response = $transfer->response();
            Assert::same($response['session'], $first['session']);
            Assert::same($response['batch'], $streamLimit);
            Assert::same($response['path'], "/barrier?group=limit&count={$streamLimit}&id={$index}");
            $streams[] = $response['stream'];
            $transfer->remove($multi);
        }
        Assert::same(count(array_unique($streams)), 12);
    } finally {
        unset($transfer, $transfers, $multi);
        gc_collect_cycles();
        $server->stop();
    }
}
echo "Done\n";
?>
--EXPECT--
Done
