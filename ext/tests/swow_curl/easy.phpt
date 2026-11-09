--TEST--
swow_curl: easy (SSL connection)
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

$certificate = __DIR__ . '/easy_server.pem';
$ca = __DIR__ . '/easy_ca.pem';
$generator = new CertificateGenerator(false);
$generator->saveCaCert($ca);
$generator->saveNewCertAsFileWithKey('localhost', $certificate, 'prime256v1', 'IP:127.0.0.1,DNS:localhost', 'critical,serverAuth');
$body1 = 'First TLS response';
$body2 = 'Second TLS response';
[$server1, $serverCoroutine1] = createHttpTestServer($certificate, body: $body1);
[$server2, $serverCoroutine2] = createHttpTestServer($certificate, body: $body2);
$url1 = 'https://127.0.0.1:' . $server1->getSockPort() . '/first';
$url2 = 'https://127.0.0.1:' . $server2->getSockPort() . '/second';

try {
    $ch1 = curl_init();
    $ch2 = curl_init();

    curl_setopt($ch1, CURLOPT_URL, $url1);
    curl_setopt_array($ch1, [CURLOPT_PROXY => '', CURLOPT_TIMEOUT_MS => 5000, CURLOPT_CAINFO => $ca, CURLOPT_SSL_VERIFYPEER => true, CURLOPT_SSL_VERIFYHOST => 2]);
    curl_setopt($ch1, CURLOPT_HEADER, 0);
    curl_setopt($ch1, CURLOPT_RETURNTRANSFER, 1);
    curl_setopt($ch2, CURLOPT_URL, $url2);
    curl_setopt_array($ch2, [CURLOPT_PROXY => '', CURLOPT_TIMEOUT_MS => 5000, CURLOPT_CAINFO => $ca, CURLOPT_SSL_VERIFYPEER => true, CURLOPT_SSL_VERIFYHOST => 2]);
    curl_setopt($ch2, CURLOPT_HEADER, 0);
    curl_setopt($ch2, CURLOPT_RETURNTRANSFER, 1);

    $response1 = curl_exec($ch1);
    $response2 = curl_exec($ch2);

    Assert::same($response1, $body1);
    Assert::same($response2, $body2);
} finally {
    unset($ch1, $ch2);
    foreach ([[$server1, $serverCoroutine1], [$server2, $serverCoroutine2]] as [$server, $serverCoroutine]) {
        $server->close();
        if ($serverCoroutine->isAlive()) {
            $serverCoroutine->kill();
        }
    }
    unlink($certificate);
    unlink($ca);
}

echo "Done\n";
?>
--CLEAN--
<?php
foreach (['easy_server.pem', 'easy_server.pem.cnf', 'easy_ca.pem'] as $file) {
    @unlink(__DIR__ . '/' . $file);
}
?>
--EXPECT--
Done
