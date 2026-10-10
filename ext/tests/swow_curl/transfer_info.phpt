--TEST--
swow_curl: typed transfer metadata matches the aggregate result
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

Assert::throws(static function (): void {
    new CurlHandle();
}, Error::class, expectMessage: '/curl_init/');
[$server, $coroutine] = createHttpTestServer();
$url = 'http://' . $server->getSockAddress() . ':' . $server->getSockPort() . '/metadata';
$handle = curl_init($url);
try {
    Assert::same((int) $handle, spl_object_id($handle));
    Assert::false(curl_getinfo($handle, CURLINFO_PRIVATE));
    $payload = "body\0binary";
    Assert::true(curl_setopt_array($handle, [
        CURLOPT_PROXY => '',
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_TIMEOUT_MS => 1000,
        CURLOPT_POSTFIELDS => $payload,
    ]));
    Assert::same(curl_exec($handle), 'Done');
    $info = curl_getinfo($handle, null);
    foreach ([
        CURLINFO_EFFECTIVE_URL => 'url',
        CURLINFO_RESPONSE_CODE => 'http_code',
        CURLINFO_HEADER_SIZE => 'header_size',
        CURLINFO_REQUEST_SIZE => 'request_size',
        CURLINFO_TOTAL_TIME => 'total_time',
        CURLINFO_NAMELOOKUP_TIME => 'namelookup_time',
        CURLINFO_CONNECT_TIME => 'connect_time',
        CURLINFO_STARTTRANSFER_TIME => 'starttransfer_time',
        CURLINFO_REDIRECT_COUNT => 'redirect_count',
        CURLINFO_PRIMARY_IP => 'primary_ip',
        CURLINFO_PRIMARY_PORT => 'primary_port',
        CURLINFO_LOCAL_IP => 'local_ip',
        CURLINFO_LOCAL_PORT => 'local_port',
        CURLINFO_HTTP_VERSION => 'http_version',
    ] as $option => $name) {
        Assert::same(curl_getinfo($handle, $option), $info[$name]);
    }
    Assert::same($info['url'], $url);
    Assert::same($info['http_version'], CURL_HTTP_VERSION_1_1);
    Assert::same(curl_getinfo($handle, CURLINFO_SIZE_UPLOAD_T), strlen($payload));
    Assert::same(curl_getinfo($handle, CURLINFO_SIZE_DOWNLOAD_T), 4);
    Assert::same(curl_getinfo($handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T), 4);
    foreach ([CURLINFO_TOTAL_TIME_T, CURLINFO_NAMELOOKUP_TIME_T, CURLINFO_CONNECT_TIME_T,
        CURLINFO_PRETRANSFER_TIME_T, CURLINFO_STARTTRANSFER_TIME_T, CURLINFO_REDIRECT_TIME_T] as $option) {
        $value = curl_getinfo($handle, $option);
        Assert::true(is_int($value));
        Assert::greaterThanEq($value, 0);
    }
    if (defined('CURLINFO_EFFECTIVE_METHOD')) {
        Assert::same(curl_getinfo($handle, CURLINFO_EFFECTIVE_METHOD), 'POST');
    }
    Assert::false(curl_getinfo($handle, CURLINFO_CONTENT_TYPE));
    Assert::same(curl_getinfo($handle, CURLINFO_CERTINFO), []);
    Assert::same(curl_getinfo($handle, CURLINFO_COOKIELIST), []);
    Assert::true(is_array(curl_getinfo($handle, CURLINFO_SSL_ENGINES)));
    Assert::false(curl_getinfo($handle, PHP_INT_MAX));
    Assert::true(curl_upkeep($handle));
    Assert::same(curl_errno($handle), CURLE_OK);
    $encoded = curl_escape($handle, "\0\xff /+");
    Assert::same($encoded, '%00%FF%20%2F%2B');
    Assert::same(curl_unescape($handle, $encoded), "\0\xff /+");
    Assert::notSame(curl_strerror(CURLE_WRITE_ERROR), '');
} finally {
    unset($handle);
    $server->close();
    if ($coroutine->isAlive()) {
        $coroutine->kill();
    }
}
echo "Done\n";
?>
--EXPECT--
Done
