--TEST--
swow_curl: debug callbacks coexist with outgoing header capture
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
skip_if_constant_not_defined('CURLOPT_DEBUGFUNCTION');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

[$server, $coroutine] = createHttpTestServer();
$handle = curl_init('http://' . $server->getSockAddress() . ':' . $server->getSockPort() . '/debug');
$stderr = tmpfile();
$records = [];
$payload = "request\0data";
try {
    Assert::true(curl_setopt_array($handle, [
        CURLOPT_PROXY => '',
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_TIMEOUT_MS => 1000,
        CURLOPT_POSTFIELDS => $payload,
        CURLOPT_STDERR => $stderr,
        CURLINFO_HEADER_OUT => true,
        CURLOPT_DEBUGFUNCTION => static function (CurlHandle $current, int $type, string $data) use ($handle, &$records): void {
            Assert::same($current, $handle);
            $records[$type] = ($records[$type] ?? '') . $data;
        },
    ]));
    Assert::same(curl_exec($handle), 'Done');
    Assert::same(curl_errno($handle), CURLE_OK);
    Assert::contains($records[CURLINFO_HEADER_OUT], 'POST /debug HTTP/1.1');
    Assert::same($records[CURLINFO_HEADER_OUT], curl_getinfo($handle, CURLINFO_HEADER_OUT));
    Assert::contains($records[CURLINFO_HEADER_IN], "HTTP/1.1 200 OK\r\n");
    Assert::same($records[CURLINFO_DATA_OUT], $payload);
    Assert::same($records[CURLINFO_DATA_IN], 'Done');
    $previous = $records;
    Assert::true(curl_setopt($handle, CURLOPT_DEBUGFUNCTION, null));
    Assert::same(curl_exec($handle), 'Done');
    Assert::same($records, $previous);
    Assert::contains(curl_getinfo($handle, CURLINFO_HEADER_OUT), 'POST /debug HTTP/1.1');
} finally {
    unset($handle);
    fclose($stderr);
    $server->close();
    if ($coroutine->isAlive()) {
        $coroutine->kill();
    }
}
echo "Done\n";
?>
--EXPECT--
Done
