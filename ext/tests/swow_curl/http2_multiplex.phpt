--TEST--
swow_curl: HTTP/2 multiplexed POST streams keep request bodies and completions independent
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
    $transfers = [];
    try {
        $first = Http2Transfer::establish($server, $multi);
        $streams = [];
        for ($batch = 0; $batch < 4; $batch++) {
            for ($index = 0; $index < 8; $index++) {
                $path = "/barrier?group={$batch}&count=8&id={$index}";
                $transfer = new Http2Transfer($server, $path, 100);
                $payload = str_repeat("request-{$batch}-{$index}\0\xff", 256);
                Assert::true(curl_setopt($transfer->handle, CURLOPT_POSTFIELDS, $payload));
                $transfer->add($multi);
                $transfers[] = $transfer;
            }
            Http2Transfer::assertSuccessful(Http2Transfer::run($multi), 8);
            foreach ($transfers as $index => $transfer) {
                $response = $transfer->response();
                $first ??= $response;
                Assert::same($response['session'], $first['session']);
                Assert::same($response['port'], $first['port']);
                Assert::same($response['batch'], 8);
                Assert::same($response['path'], "/barrier?group={$batch}&count=8&id={$index}");
                Assert::same($response['method'], 'POST');
                Assert::same(base64_decode($response['body'], true), str_repeat("request-{$batch}-{$index}\0\xff", 256));
                Assert::false(isset($streams[$response['stream']]));
                $streams[$response['stream']] = true;
                $transfer->remove($multi);
            }
            $transfers = [];
        }
        Assert::same(count($streams), 32);
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
