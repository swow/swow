--TEST--
swow_curl: rejected callback results release their temporary references
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

[$server, $coroutine] = createHttpTestServer();
$url = 'http://' . $server->getSockAddress() . ':' . $server->getSockPort() . '/callback-result';
$handle = curl_init($url);
try {
    Assert::true(curl_setopt_array($handle, [
        CURLOPT_PROXY => '',
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_TIMEOUT_MS => 1000,
    ]));
    $reference = null;
    Assert::true(curl_setopt($handle, CURLOPT_WRITEFUNCTION, static function (CurlHandle $current, string $data) use (&$reference): array {
        $value = new stdClass();
        $reference = WeakReference::create($value);
        return [$value];
    }));
    // An array coerces to a short write count; rejected data must not retain its temporary objects.
    Assert::false(curl_exec($handle));
    Assert::same(curl_errno($handle), CURLE_WRITE_ERROR);
    Assert::isInstanceOf($reference, WeakReference::class);
    Assert::null($reference->get());

    curl_reset($handle);
    Assert::true(curl_setopt_array($handle, [
        CURLOPT_URL => $url,
        CURLOPT_PROXY => '',
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_TIMEOUT_MS => 1000,
    ]));
    if (defined('CURLOPT_PREREQFUNCTION')) {
        $reference = null;
        Assert::true(curl_setopt($handle, CURLOPT_PREREQFUNCTION, static function (CurlHandle $current, string $primaryIp, string $localIp, int $primaryPort, int $localPort) use (&$reference): array {
            $value = new stdClass();
            $reference = WeakReference::create($value);
            return [$value];
        }));
        Assert::throws(static function () use ($handle): void {
            curl_exec($handle);
        }, TypeError::class, expectMessage: '/CURL_PREREQFUNC_OK or CURL_PREREQFUNC_ABORT/');
        Assert::isInstanceOf($reference, WeakReference::class);
        Assert::null($reference->get());
        Assert::true(curl_setopt($handle, CURLOPT_PREREQFUNCTION, null));
    }
    Assert::same(curl_exec($handle), 'Done');
    Assert::same(curl_errno($handle), CURLE_OK);
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
