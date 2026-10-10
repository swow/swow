--TEST--
swow_curl: progress, prerequisite and response callbacks control real transfers
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

[$server, $coroutine] = createHttpTestServer();
$url = 'http://' . $server->getSockAddress() . ':' . $server->getSockPort() . '/control';
$handle = curl_init();
try {
    foreach ([CURLOPT_PROGRESSFUNCTION, CURLOPT_XFERINFOFUNCTION] as $option) {
        foreach ([false, true] as $abort) {
            curl_reset($handle);
            $calls = 0;
            Assert::true(curl_setopt_array($handle, [
                CURLOPT_URL => $url,
                CURLOPT_PROXY => '',
                CURLOPT_RETURNTRANSFER => true,
                CURLOPT_TIMEOUT_MS => 1000,
                CURLOPT_NOPROGRESS => false,
                $option => static function (CurlHandle $current, int $downloadTotal, int $downloaded, int $uploadTotal, int $uploaded) use ($handle, $abort, &$calls): int {
                    Assert::same($current, $handle);
                    Assert::greaterThanEq($downloadTotal, 0);
                    Assert::greaterThanEq($downloaded, 0);
                    Assert::greaterThanEq($uploadTotal, 0);
                    Assert::greaterThanEq($uploaded, 0);
                    $calls++;
                    return $abort ? 1 : 0;
                },
            ]));
            Assert::same(curl_exec($handle), $abort ? false : 'Done');
            Assert::same(curl_errno($handle), $abort ? CURLE_ABORTED_BY_CALLBACK : CURLE_OK);
            Assert::greaterThan($calls, 0);
        }
    }

    foreach ([CURLOPT_HEADERFUNCTION, CURLOPT_WRITEFUNCTION] as $option) {
        curl_reset($handle);
        Assert::true(curl_setopt_array($handle, [
            CURLOPT_URL => $url,
            CURLOPT_PROXY => '',
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_TIMEOUT_MS => 1000,
            $option => static fn(CurlHandle $current, string $data): int => 0,
        ]));
        Assert::false(curl_exec($handle));
        Assert::same(curl_errno($handle), CURLE_WRITE_ERROR);
        Assert::notSame(curl_error($handle), '');
        Assert::true(curl_setopt($handle, $option, null));
        Assert::true(curl_setopt($handle, CURLOPT_RETURNTRANSFER, true));
        Assert::same(curl_exec($handle), 'Done');
    }

    if (defined('CURLOPT_PREREQFUNCTION')) {
        foreach ([CURL_PREREQFUNC_OK, CURL_PREREQFUNC_ABORT, 2] as $result) {
            curl_reset($handle);
            $calls = 0;
            Assert::true(curl_setopt_array($handle, [
                CURLOPT_URL => $url,
                CURLOPT_PROXY => '',
                CURLOPT_RETURNTRANSFER => true,
                CURLOPT_TIMEOUT_MS => 1000,
                CURLOPT_PREREQFUNCTION => static function (CurlHandle $current, string $primaryIp, string $localIp, int $primaryPort, int $localPort) use ($handle, $server, $result, &$calls): int {
                    Assert::same($current, $handle);
                    Assert::same($primaryIp, '127.0.0.1');
                    Assert::same($localIp, '127.0.0.1');
                    Assert::same($primaryPort, $server->getSockPort());
                    Assert::greaterThan($localPort, 0);
                    $calls++;
                    return $result;
                },
            ]));
            if ($result === 2) {
                Assert::throws(static function () use ($handle): void {
                    curl_exec($handle);
                }, ValueError::class, expectMessage: '/CURL_PREREQFUNC_OK or CURL_PREREQFUNC_ABORT/');
            } else {
                Assert::same(curl_exec($handle), $result === CURL_PREREQFUNC_OK ? 'Done' : false);
                Assert::same(curl_errno($handle), $result === CURL_PREREQFUNC_OK ? CURLE_OK : CURLE_ABORTED_BY_CALLBACK);
            }
            Assert::same($calls, 1);
        }
    }
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
