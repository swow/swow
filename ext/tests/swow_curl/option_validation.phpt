--TEST--
swow_curl: invalid option values do not prevent later valid transfers
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

[$server, $coroutine] = createHttpTestServer();
$url = 'http://' . $server->getSockAddress() . ':' . $server->getSockPort() . '/validation';
$handle = curl_init($url);
$readOnly = fopen(__FILE__, 'rb');
try {
    foreach ([
        CURLOPT_HTTPHEADER, CURLOPT_QUOTE, CURLOPT_HTTP200ALIASES, CURLOPT_POSTQUOTE,
        CURLOPT_PREQUOTE, CURLOPT_TELNETOPTIONS, CURLOPT_MAIL_RCPT, CURLOPT_RESOLVE,
        CURLOPT_PROXYHEADER, CURLOPT_CONNECT_TO,
    ] as $option) {
        Assert::throws(static function () use ($handle, $option): void {
            curl_setopt($handle, $option, 'not an array');
        }, TypeError::class, expectMessage: '/must have an array value/');
        Assert::true(curl_setopt($handle, $option, []));
    }
    $callbacks = [CURLOPT_READFUNCTION, CURLOPT_WRITEFUNCTION, CURLOPT_HEADERFUNCTION,
        CURLOPT_PROGRESSFUNCTION, CURLOPT_XFERINFOFUNCTION, CURLOPT_FNMATCH_FUNCTION];
    foreach (['CURLOPT_DEBUGFUNCTION', 'CURLOPT_PREREQFUNCTION'] as $name) {
        if (defined($name)) {
            $callbacks[] = constant($name);
        }
    }
    foreach ($callbacks as $option) {
        Assert::throws(static function () use ($handle, $option): void {
            curl_setopt($handle, $option, 'swow_missing_callback');
        }, TypeError::class, expectMessage: '/valid callback/');
        Assert::true(curl_setopt($handle, $option, null));
    }
    foreach ([CURLOPT_FILE, CURLOPT_WRITEHEADER] as $option) {
        Assert::throws(static function () use ($handle, $option, $readOnly): void {
            curl_setopt($handle, $option, $readOnly);
        }, ValueError::class, expectMessage: '/must be writable/');
    }
    Assert::throws(static function () use ($handle): void {
        curl_setopt($handle, CURLOPT_SAFE_UPLOAD, false);
    }, ValueError::class, expectMessage: '/Disabling safe uploads/');
    Assert::throws(static function () use ($handle): void {
        curl_setopt($handle, CURLOPT_URL, "http://example.test/\0path");
    }, ValueError::class, expectMessage: '/null bytes/');
    Assert::throws(static function () use ($handle): void {
        curl_setopt($handle, PHP_INT_MAX, true);
    }, ValueError::class, expectMessage: '/valid cURL option/');

    curl_reset($handle);
    Assert::true(curl_setopt_array($handle, [
        CURLOPT_URL => $url,
        CURLOPT_PROXY => '',
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_TIMEOUT_MS => 1000,
        CURLINFO_HEADER_OUT => true,
    ]));
    Assert::throws(static function () use ($handle): void {
        curl_setopt_array($handle, [CURLOPT_HTTPHEADER => ['X-Before: retained'], 'invalid' => true]);
    }, ValueError::class, expectMessage: '/invalid cURL option/');
    Assert::same(curl_exec($handle), 'Done');
    Assert::same(curl_errno($handle), CURLE_OK);
    // An invalid later array key does not roll back options already applied by curl_setopt_array.
    Assert::contains(curl_getinfo($handle, CURLINFO_HEADER_OUT), "\r\nX-Before: retained\r\n");
} finally {
    unset($handle);
    fclose($readOnly);
    $server->close();
    if ($coroutine->isAlive()) {
        $coroutine->kill();
    }
}
echo "Done\n";
?>
--EXPECT--
Done
