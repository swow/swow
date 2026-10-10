--TEST--
swow_curl: file output, header output, diagnostic streams and reset ownership
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

[$server, $coroutine] = createHttpTestServer();
$handle = curl_init('http://' . $server->getSockAddress() . ':' . $server->getSockPort() . '/files');
$streams = [];
try {
    foreach ([CURLOPT_FILE, CURLOPT_WRITEHEADER, CURLOPT_STDERR, CURLOPT_INFILE] as $option) {
        $streams[$option] = tmpfile();
        Assert::true(curl_setopt($handle, $option, $streams[$option]));
    }
    Assert::true(curl_setopt_array($handle, [
        CURLOPT_PROXY => '',
        CURLOPT_VERBOSE => true,
        CURLOPT_TIMEOUT_MS => 1000,
    ]));
    Assert::true(curl_exec($handle));
    rewind($streams[CURLOPT_FILE]);
    Assert::same(stream_get_contents($streams[CURLOPT_FILE]), 'Done');
    rewind($streams[CURLOPT_WRITEHEADER]);
    $headers = stream_get_contents($streams[CURLOPT_WRITEHEADER]);
    Assert::contains($headers, "HTTP/1.1 200 OK\r\n");
    Assert::contains($headers, "Content-Length: 4\r\n");
    rewind($streams[CURLOPT_STDERR]);
    Assert::contains(stream_get_contents($streams[CURLOPT_STDERR]), 'GET /files HTTP/1.1');

    $paths = [];
    foreach ($streams as $stream) {
        $paths[] = stream_get_meta_data($stream)['uri'];
    }
    unset($stream, $streams);
    // The handle owns all configured streams after the caller releases them.
    foreach ($paths as $path) {
        Assert::true(file_exists($path));
    }
    curl_reset($handle);
    foreach ($paths as $path) {
        clearstatcache(true, $path);
        Assert::false(file_exists($path));
    }

    foreach ([CURLOPT_FILE, CURLOPT_WRITEHEADER, CURLOPT_STDERR, CURLOPT_INFILE] as $option) {
        $stream = tmpfile();
        $path = stream_get_meta_data($stream)['uri'];
        Assert::true(curl_setopt($handle, $option, $stream));
        unset($stream);
        Assert::true(file_exists($path));
        Assert::true(curl_setopt($handle, $option, null));
        clearstatcache(true, $path);
        Assert::false(file_exists($path));
    }
} finally {
    unset($handle);
    foreach ($streams ?? [] as $stream) {
        if (is_resource($stream)) {
            fclose($stream);
        }
    }
    $server->close();
    if ($coroutine->isAlive()) {
        $coroutine->kill();
    }
}
echo "Done\n";
?>
--EXPECT--
Done
