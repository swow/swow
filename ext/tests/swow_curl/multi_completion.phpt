--TEST--
swow_curl: multi completion identifies successful and failed transfer handles
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

[$server, $coroutine] = createHttpTestServer();
$multi = curl_multi_init();
$handles = [];
try {
    Assert::true(curl_multi_setopt($multi, CURLMOPT_MAX_TOTAL_CONNECTIONS, 2));
    Assert::same(curl_multi_get_handles($multi), []);
    Assert::false(curl_multi_info_read($multi));
    foreach (['/first', '/second', null] as $path) {
        $handle = curl_init($path === null ? 'invalid-scheme://example.test/' : 'http://' . $server->getSockAddress() . ':' . $server->getSockPort() . $path);
        Assert::true(curl_setopt_array($handle, [
            CURLOPT_PROXY => '',
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_TIMEOUT_MS => 1000,
        ]));
        $handles[] = $handle;
        Assert::same(curl_multi_add_handle($multi, $handle), CURLM_OK);
    }
    Assert::same(curl_multi_get_handles($multi), $handles);
    do {
        $result = curl_multi_exec($multi, $running);
        Assert::same($result, CURLM_OK);
        if ($running > 0) {
            Assert::greaterThanEq(curl_multi_select($multi, 0.1), 0);
        }
    } while ($running > 0);
    $completed = [];
    $remaining = [];
    while (($message = curl_multi_info_read($multi, $queued)) !== false) {
        Assert::same($message['msg'], CURLMSG_DONE);
        $completed[spl_object_id($message['handle'])] = $message['result'];
        $remaining[] = $queued;
    }
    Assert::same(count($completed), 3);
    Assert::same($remaining, [2, 1, 0]);
    foreach ([$handles[0], $handles[1]] as $handle) {
        Assert::same($completed[spl_object_id($handle)], CURLE_OK);
        Assert::same(curl_multi_getcontent($handle), 'Done');
        Assert::same(curl_getinfo($handle, CURLINFO_HTTP_CODE), 200);
    }
    Assert::same($completed[spl_object_id($handles[2])], CURLE_UNSUPPORTED_PROTOCOL);
    Assert::same(curl_errno($handles[2]), CURLE_UNSUPPORTED_PROTOCOL);
    Assert::notSame(curl_error($handles[2]), '');
    Assert::same(curl_multi_errno($multi), CURLM_OK);
    foreach ($handles as $handle) {
        Assert::same(curl_multi_remove_handle($multi, $handle), CURLM_OK);
    }
    Assert::same(curl_multi_get_handles($multi), []);
    Assert::false(curl_multi_info_read($multi));
} finally {
    foreach (curl_multi_get_handles($multi) as $handle) {
        curl_multi_remove_handle($multi, $handle);
    }
    unset($handle, $handles, $multi);
    $server->close();
    if ($coroutine->isAlive()) {
        $coroutine->kill();
    }
}

echo "Done\n";
?>
--EXPECT--
Done
