--TEST--
swow_curl: closing and collecting active HTTP/2 multi handles release callbacks and allow new requests
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
    try {
        foreach (['close', 'collect'] as $operation) {
            $multi = Http2Transfer::multi();
            $first = new Http2Transfer($server, "/hold?group={$operation}&count=2&id=first");
            $second = new Http2Transfer($server, "/hold?group={$operation}&count=2&id=second");
            $first->add($multi);
            $second->add($multi);
            Http2Transfer::until($multi, static fn(): bool => str_contains($first->headers, 'x-stream:') && str_contains($second->headers, 'x-stream:'));
            $references = [WeakReference::create($first), WeakReference::create($second), WeakReference::create($first->handle), WeakReference::create($second->handle), WeakReference::create($multi)];
            if ($operation === 'close') {
                curl_multi_close($multi);
                Assert::same(curl_multi_get_handles($multi), []);
            }
            unset($first, $second, $multi);
            gc_collect_cycles();
            foreach ($references as $reference) {
                Assert::null($reference->get());
            }
            $multi = Http2Transfer::multi();
            $transfer = new Http2Transfer($server, '/after-' . $operation);
            $transfer->add($multi);
            Http2Transfer::assertSuccessful(Http2Transfer::run($multi), 1);
            Assert::same($transfer->response()['path'], '/after-' . $operation);
            $transfer->remove($multi);
            unset($transfer, $multi);
            gc_collect_cycles();
        }
    } finally {
        unset($first, $second, $transfer, $multi);
        gc_collect_cycles();
        $server->stop();
    }
}
echo "Done\n";
?>
--EXPECT--
Done
