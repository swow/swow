--TEST--
swow_curl: HTTP/2 yielding callbacks preserve binary bodies and trailers across flow-control windows
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
        $connection = Http2Transfer::establish($server, $multi);
        $size = 256 * 1024 + 17;
        for ($index = 0; $index < 6; $index++) {
            $transfer = new Http2Transfer($server, "/data?group=data&count=6&id={$index}&size={$size}", 100);
            $transfer->add($multi);
            $transfers[] = $transfer;
        }
        Http2Transfer::assertSuccessful(Http2Transfer::run($multi), 6);
        $sessions = [];
        $streams = [];
        foreach ($transfers as $index => $transfer) {
            $expected = substr(str_repeat("{$index}\0\xff", intdiv($size, 3) + 1), 0, $size);
            Assert::same($transfer->body, $expected);
            Assert::greaterThan($transfer->writes, 1);
            Assert::same(preg_match('/HTTP\/2 200 ?\r\n/', $transfer->headers), 1);
            Assert::contains($transfer->headers, 'x-body-sha256: ' . hash('sha256', $expected));
            Assert::same(preg_match('/x-session: (\d+)/', $transfer->headers, $match), 1);
            Assert::same((int) $match[1], $connection['session']);
            $sessions[] = $match[1];
            Assert::same(preg_match('/x-stream: (\d+)/', $transfer->headers, $match), 1);
            $streams[] = $match[1];
            $transfer->remove($multi);
        }
        Assert::same(count(array_unique($sessions)), 1);
        Assert::same(count(array_unique($streams)), 6);
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
