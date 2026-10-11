--TEST--
swow_curl: HTTP/2 peer reset and write callback failure do not contaminate sibling streams
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
        Http2Transfer::establish($server, $multi);
        foreach (['reset', 'callback'] as $operation) {
            $path = $operation === 'reset' ? '/reset' : '/data?size=131072&id=failed';
            $path .= ($operation === 'reset' ? '?' : '&') . "group={$operation}&count=2";
            $failed = new Http2Transfer($server, $path);
            if ($operation === 'callback') {
                Assert::true(curl_setopt($failed->handle, CURLOPT_WRITEFUNCTION, static function (CurlHandle $handle, string $data): int {
                    usleep(100);
                    return 0;
                }));
            }
            $sibling = new Http2Transfer($server, "/barrier?group={$operation}&count=2", 100);
            $failed->add($multi);
            $sibling->add($multi);
            $messages = Http2Transfer::run($multi);
            Assert::same(count($messages), 2);
            $results = [];
            foreach ($messages as $message) {
                $results[spl_object_id($message['handle'])] = $message['result'];
            }
            if ($operation === 'reset') {
                $result = $results[spl_object_id($failed->handle)];
                Assert::notSame($result, CURLE_OK);
                // libcurl can report the incomplete body or the stream reset; PHP has no CURLE_HTTP2_STREAM constant.
                if ($result !== CURLE_PARTIAL_FILE) {
                    Assert::contains(strtolower(curl_strerror($result)), 'stream');
                }
                Assert::same($failed->body, '');
            } else {
                Assert::same($results[spl_object_id($failed->handle)], CURLE_WRITE_ERROR);
            }
            Assert::notSame(curl_error($failed->handle), '');
            Assert::same($results[spl_object_id($sibling->handle)], CURLE_OK);
            $response = $sibling->response();
            Assert::same($response['batch'], 2);
            Assert::same($response['path'], "/barrier?group={$operation}&count=2");
            $failed->remove($multi);
            $sibling->remove($multi);
            $recovery = new Http2Transfer($server, '/after-' . $operation);
            $recovery->add($multi);
            Http2Transfer::assertSuccessful(Http2Transfer::run($multi), 1);
            Assert::same($recovery->response()['session'], $response['session']);
            $recovery->remove($multi);
        }
    } finally {
        unset($failed, $sibling, $recovery, $messages, $message, $multi);
        gc_collect_cycles();
        $server->stop();
    }
}
echo "Done\n";
?>
--EXPECT--
Done
