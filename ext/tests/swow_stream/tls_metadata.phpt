--TEST--
swow_stream: encrypted private keys, verified peer certificates and TLS metadata
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!extension_loaded('openssl'), 'openssl extension is required');
skip_if(!Swow\Extension::isBuiltWith('openssl'), 'extension must be built with ssl');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

$certificate = __DIR__ . '/tls_metadata_server.pem';
$ca = __DIR__ . '/tls_metadata_ca.pem';
$generator = new CertificateGenerator(false);
$generator->saveCaCert($ca);
$generator->saveNewCertAsFileWithKey('localhost', $certificate, 'prime256v1', 'IP:127.0.0.1,DNS:localhost', 'critical,serverAuth');
$pem = file_get_contents($certificate);
$key = openssl_pkey_get_private($pem);
Assert::true(openssl_pkey_export($key, $encryptedKey, 'test-password', [
    'encrypt_key' => true,
    'cipher' => OPENSSL_CIPHER_AES_256_CBC,
]));
Assert::true(openssl_x509_export($pem, $certificatePem));
file_put_contents($certificate, $certificatePem . $encryptedKey);
$serverContext = stream_context_create(['ssl' => [
    'local_cert' => $certificate,
    'local_pk' => $certificate,
    'passphrase' => 'test-password',
    'verify_peer' => false,
    'crypto_method' => STREAM_CRYPTO_METHOD_TLSv1_2_SERVER,
    'alpn_protocols' => 'http/1.1',
]]);
$server = stream_socket_server('tls://127.0.0.1:0', context: $serverContext);
$client = null;
$serverCrypto = null;
$coroutine = Coroutine::run(static function () use ($server, &$serverCrypto): void {
    $connection = stream_socket_accept($server, 1);
    try {
        $serverCrypto = stream_get_meta_data($connection)['crypto'];
        Assert::same(fwrite($connection, 'hello'), 5);
    } finally {
        fclose($connection);
    }
});
try {
    $context = stream_context_create(['ssl' => [
        'cafile' => $ca,
        'peer_name' => 'localhost',
        'verify_peer' => true,
        'verify_peer_name' => true,
        'verify_depth' => 2,
        'allow_self_signed' => false,
        'capture_peer_cert' => true,
        'capture_peer_cert_chain' => true,
        'crypto_method' => STREAM_CRYPTO_METHOD_TLSv1_2_CLIENT,
        'alpn_protocols' => 'h2,http/1.1',
    ]]);
    $client = stream_socket_client('tls://' . stream_socket_get_name($server, false), timeout: 1, context: $context);
    Assert::true(stream_set_timeout($client, 1));
    Assert::same(fread($client, 5), 'hello');
    $metadata = stream_get_meta_data($client);
    Assert::false($metadata['timed_out']);
    Assert::same($metadata['crypto']['protocol'], 'TLSv1.2');
    Assert::notEmpty($metadata['crypto']['cipher_name']);
    Assert::greaterThanEq($metadata['crypto']['cipher_bits'], 128);
    Assert::same($metadata['crypto']['alpn_protocol'], 'http/1.1');
    Assert::same($serverCrypto['protocol'], 'TLSv1.2');
    Assert::same($serverCrypto['alpn_protocol'], 'http/1.1');
    $ssl = stream_context_get_options($client)['ssl'];
    Assert::isInstanceOf($ssl['peer_certificate'], OpenSSLCertificate::class);
    Assert::same(openssl_x509_fingerprint($ssl['peer_certificate'], 'sha256'), $generator->getCertDigest('sha256'));
    Assert::notEmpty($ssl['peer_certificate_chain']);
    Assert::same(openssl_x509_fingerprint($ssl['peer_certificate_chain'][0], 'sha256'), $generator->getCertDigest('sha256'));
} finally {
    if (is_resource($client)) {
        fclose($client);
    }
    if ($coroutine->isAlive()) {
        $coroutine->kill();
    }
    fclose($server);
    unlink($certificate);
    unlink($ca);
}
echo "Done\n";
?>
--CLEAN--
<?php
foreach (['tls_metadata_server.pem', 'tls_metadata_server.pem.cnf', 'tls_metadata_ca.pem'] as $file) {
    @unlink(__DIR__ . '/' . $file);
}
?>
--EXPECT--
Done
