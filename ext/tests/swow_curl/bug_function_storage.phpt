--TEST--
swow_curl: callback function storage
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_SAPI !== 'cli', 'only for cli');
skip_if(!Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
skip_if(PHP_OS === 'Darwin', 'curl hook is not working properly on macOS');
skip_if(!Swow\Extension::isBuiltWith('openssl'), 'extension must be built with ssl');
skip_if(!extension_loaded('openssl'), 'openssl extension is required');
?>
--FILE--
<?php
require_once __DIR__ . '/../include/bootstrap.php';

$certificate = __DIR__ . '/bug_function_storage_server.pem';
$ca = __DIR__ . '/bug_function_storage_ca.pem';
$generator = new CertificateGenerator(false);
$generator->saveCaCert($ca);
$generator->saveNewCertAsFileWithKey('localhost', $certificate, 'prime256v1', 'IP:127.0.0.1,DNS:localhost', 'critical,serverAuth');
$body = 'Local TLS response';
[$server, $serverCoroutine] = createHttpTestServer($certificate, body: $body, headers: ['Date' => gmdate('D, d M Y H:i:s \G\M\T')]);
$url = 'https://127.0.0.1:' . $server->getSockPort() . '/transfer';

$testHeaderFunction = static function (CurlHandle $curl, string $headerLine): int {
    return testHeaderFunction($curl, $headerLine);
};

function testHeaderFunction(CurlHandle $curl, string $headerLine): int
{
    $parts = explode(':', $headerLine, 2);
    if (count($parts) === 2) {
        $GLOBALS['header_lines'][strtolower($parts[0])] = trim($parts[1]);
    }
    return strlen($headerLine);
}

class testHeaderFunctionClass
{
    public static function headerFunction(CurlHandle $curl, string $headerLine): int
    {
        return testHeaderFunction($curl, $headerLine);
    }
}

try {
    foreach ([$testHeaderFunction, 'testHeaderFunction', [testHeaderFunctionClass::class, 'headerFunction']] as $headerFunction) {
        $curl = curl_init();
        curl_setopt($curl, CURLOPT_URL, $url);
        curl_setopt_array($curl, [CURLOPT_PROXY => '', CURLOPT_TIMEOUT_MS => 5000, CURLOPT_CAINFO => $ca, CURLOPT_SSL_VERIFYPEER => true, CURLOPT_SSL_VERIFYHOST => 2]);
        curl_setopt($curl, CURLOPT_HEADERFUNCTION, $headerFunction);
        curl_setopt($curl, CURLOPT_RETURNTRANSFER, 1);
        unset($testHeaderFunction);
        $response = curl_exec($curl);
        Assert::same($response, $body);
        Assert::true(strtotime($GLOBALS['header_lines']['date']) > 0);
        if (PHP_VERSION_ID < 80100) {
            curl_close($curl);
        }
    }

    $curl = curl_init();
    curl_setopt($curl, CURLOPT_URL, $url);
    curl_setopt_array($curl, [CURLOPT_PROXY => '', CURLOPT_TIMEOUT_MS => 5000, CURLOPT_CAINFO => $ca, CURLOPT_SSL_VERIFYPEER => true, CURLOPT_SSL_VERIFYHOST => 2]);
    curl_setopt($curl, CURLOPT_HEADERFUNCTION, 'testHeaderFunction');
    curl_setopt($curl, CURLOPT_HEADERFUNCTION, null);
} finally {
    unset($curl);
    $server->close();
    if ($serverCoroutine->isAlive()) {
        $serverCoroutine->kill();
    }
    unlink($certificate);
    unlink($ca);
}

echo "Done\n";
?>
--CLEAN--
<?php
foreach (['bug_function_storage_server.pem', 'bug_function_storage_server.pem.cnf', 'bug_function_storage_ca.pem'] as $file) {
    @unlink(__DIR__ . '/' . $file);
}
?>
--EXPECT--
Done
