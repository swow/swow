--TEST--
swow_curl: callback ownership, copied handles, transfer metadata and reset
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

[$server, $coroutine] = createHttpTestServer();
$url = 'http://' . $server->getSockAddress() . ':' . $server->getSockPort() . '/callbacks';
$handle = curl_init($url);
$copy = null;
try {
    Assert::true(curl_setopt_array($handle, [
        CURLOPT_PROXY => '',
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_TIMEOUT_MS => 1000,
        CURLOPT_PRIVATE => 'original',
    ]));
    Assert::same(curl_exec($handle), 'Done');
    Assert::same(curl_errno($handle), CURLE_OK);
    Assert::same(curl_error($handle), '');
    $info = curl_getinfo($handle);
    Assert::same($info['url'], $url);
    Assert::same($info['http_code'], 200);
    Assert::same($info['primary_ip'], '127.0.0.1');
    Assert::same($info['primary_port'], $server->getSockPort());
    Assert::same(curl_getinfo($handle, CURLINFO_HTTP_CODE), $info['http_code']);
    Assert::same(curl_getinfo($handle, CURLINFO_TOTAL_TIME), $info['total_time']);
    Assert::same(curl_getinfo($handle, CURLINFO_PRIVATE), 'original');
    Assert::same(curl_getinfo($handle, CURLINFO_SIZE_DOWNLOAD_T), 4);
    $encoded = curl_escape($handle, 'a b/+');
    Assert::same($encoded, 'a%20b%2F%2B');
    Assert::same(curl_unescape($handle, $encoded), 'a b/+');

    $body = '';
    $headers = '';
    $callbackHandle = null;
    Assert::true(curl_setopt($handle, CURLOPT_WRITEFUNCTION, static function (CurlHandle $current, string $data) use (&$body, &$callbackHandle): int {
        $callbackHandle = $current;
        $body .= $data;
        return strlen($data);
    }));
    Assert::true(curl_setopt($handle, CURLOPT_HEADERFUNCTION, static function (CurlHandle $current, string $data) use (&$headers): int {
        $headers .= $data;
        return strlen($data);
    }));
    $copy = curl_copy_handle($handle);
    Assert::notSame($copy, $handle);
    Assert::true(curl_setopt($copy, CURLOPT_PRIVATE, 'copy'));
    // The copied handle owns its callbacks after the original handle is released.
    unset($handle);
    Assert::true(curl_exec($copy));
    Assert::same($callbackHandle, $copy);
    Assert::same($body, 'Done');
    Assert::contains($headers, "HTTP/1.1 200 OK\r\n");
    Assert::contains($headers, "Content-Length: 4\r\n");
    Assert::same(curl_getinfo($copy, CURLINFO_PRIVATE), 'copy');

    curl_reset($copy);
    Assert::true(curl_setopt_array($copy, [
        CURLOPT_URL => $url,
        CURLOPT_PROXY => '',
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_TIMEOUT_MS => 1000,
    ]));
    Assert::same(curl_exec($copy), 'Done');
    Assert::same($body, 'Done');
    Assert::same(curl_errno($copy), CURLE_OK);
} finally {
    unset($handle, $copy, $callbackHandle);
    $server->close();
    if ($coroutine->isAlive()) {
        $coroutine->kill();
    }
}

echo "Done\n";
?>
--EXPECT--
Done
