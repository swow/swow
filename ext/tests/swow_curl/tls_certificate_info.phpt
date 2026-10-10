--TEST--
swow_curl: verified local TLS connections expose certificate metadata
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
skip_if(!Swow\Extension::isBuiltWith('openssl'), 'extension must be built with ssl');
skip_if(!extension_loaded('openssl'), 'openssl extension is required');
$version = curl_version();
skip_if(!preg_match('/OpenSSL|LibreSSL|BoringSSL|GnuTLS|Schannel|rustls/i', $version['ssl_version']), 'libcurl TLS backend must support certificate information');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$certificate = __DIR__ . '/tls_certificate_info_server.pem';
$ca = __DIR__ . '/tls_certificate_info_ca.pem';
$generator = new CertificateGenerator(false);
$generator->saveCaCert($ca);
$generator->saveNewCertAsFileWithKey('localhost', $certificate, 'prime256v1', 'IP:127.0.0.1,DNS:localhost', 'critical,serverAuth');
[$server, $coroutine] = createHttpTestServer($certificate);
$handle = curl_init('https://127.0.0.1:' . $server->getSockPort() . '/certificate');
try {
    Assert::same(curl_getinfo($handle, CURLINFO_CERTINFO), []);
    Assert::true(curl_setopt_array($handle, [
        CURLOPT_PROXY => '',
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_TIMEOUT_MS => 1000,
        CURLOPT_CAINFO => $ca,
        CURLOPT_SSL_VERIFYPEER => true,
        CURLOPT_SSL_VERIFYHOST => 2,
        CURLOPT_CERTINFO => true,
    ]));
    Assert::same(curl_exec($handle), 'Done');
    Assert::same(curl_errno($handle), CURLE_OK);
    Assert::same(curl_getinfo($handle, CURLINFO_SSL_VERIFYRESULT), 0);
    $certificates = curl_getinfo($handle, CURLINFO_CERTINFO);
    Assert::isArray($certificates);
    Assert::notEmpty($certificates);
    Assert::same(openssl_x509_fingerprint($certificates[0]['Cert'], 'sha256'), $generator->getCertDigest('sha256'));
    Assert::same(curl_getinfo($handle)['certinfo'], $certificates);
    // Certificate metadata belongs to the transfer; resetting must release its previous chain.
    curl_reset($handle);
    Assert::same(curl_getinfo($handle, CURLINFO_CERTINFO), []);
} finally {
    unset($handle);
    $server->close();
    if ($coroutine->isAlive()) {
        $coroutine->kill();
    }
    unlink($certificate);
    unlink($ca);
}
echo "Done\n";
?>
--CLEAN--
<?php
foreach (['tls_certificate_info_server.pem', 'tls_certificate_info_server.pem.cnf', 'tls_certificate_info_ca.pem'] as $file) {
    @unlink(__DIR__ . '/' . $file);
}
?>
--EXPECT--
Done
