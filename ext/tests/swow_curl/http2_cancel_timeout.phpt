--TEST--
swow_curl: HTTP/2 cancellation and timeout leave sibling streams and connection reuse intact
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
        foreach (['cancel', 'timeout'] as $operation) {
            $held = new Http2Transfer($server, "/hold?group={$operation}&count=2");
            $sibling = new Http2Transfer($server, "/barrier?group={$operation}&count=2");
            if ($operation === 'timeout') {
                Assert::true(curl_setopt($held->handle, CURLOPT_TIMEOUT_MS, 500));
            }
            $held->add($multi);
            $sibling->add($multi);
            if ($operation === 'cancel') {
                Http2Transfer::until($multi, static fn(): bool => str_contains($held->headers, 'x-stream:') && $sibling->body !== '');
                // The server has accepted this stream and its sibling has made progress before cancellation.
                $held->remove($multi);
                Http2Transfer::assertSuccessful(Http2Transfer::run($multi), 1);
            } else {
                $messages = Http2Transfer::run($multi);
                Assert::same(count($messages), 2);
                $results = [];
                foreach ($messages as $message) {
                    $results[spl_object_id($message['handle'])] = $message['result'];
                }
                Assert::same($results[spl_object_id($held->handle)], CURLE_OPERATION_TIMEDOUT);
                Assert::same($results[spl_object_id($sibling->handle)], CURLE_OK);
                Assert::contains($held->headers, 'x-stream:');
                $held->remove($multi);
            }
            $response = $sibling->response();
            Assert::same($response['path'], "/barrier?group={$operation}&count=2");
            Assert::contains($held->headers, 'x-session: ' . $response['session']);
            $sibling->remove($multi);
            $recovery = new Http2Transfer($server, '/after-' . $operation);
            $recovery->add($multi);
            Http2Transfer::assertSuccessful(Http2Transfer::run($multi), 1);
            $next = $recovery->response();
            Assert::same($next['session'], $response['session']);
            Assert::same($next['port'], $response['port']);
            Assert::greaterThan($next['stream'], $response['stream']);
            $recovery->remove($multi);
        }
    } finally {
        unset($held, $sibling, $recovery, $messages, $message, $multi);
        gc_collect_cycles();
        $server->stop();
    }
}
echo "Done\n";
?>
--EXPECT--
Done
