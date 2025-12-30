--TEST--
swow_stream: tls peer fingerprint
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!extension_loaded('openssl'), 'openssl extension is required');
skip_if(!Swow\Extension::isBuiltWith('openssl'), 'extension must be built with ssl');
?>
--FILE--
<?php
require_once __DIR__ . '/../include/bootstrap.php';

$certGenerator = new CertificateGenerator(false);

$certGenerator->saveCaCert(__DIR__ . '/tls_peer_fingerprint_ca.crt');
$certGenerator->saveNewCertAsFileWithKey(
    'server',
    __DIR__ . '/tls_peer_fingerprint_server.pem',
    'prime256v1',
    'IP:127.0.0.1,DNS:localhost',
    'critical,serverAuth'
);
$serverFingerprints = [];
foreach (['md5', 'sha1', 'sha256', 'sha512', 'sha3-256', 'sm3', 'blake2b512'] as $algo) {
    if (in_array($algo, openssl_get_md_methods(), true)) {
        $serverFingerprints[$algo] = $certGenerator->getCertDigest($algo);
    }
}
$badServerFingerprints = [];
foreach ($serverFingerprints as $algo => $fingerprint) {
    $badServerFingerprints[$algo] = $fingerprint;
    if ($fingerprint[0] === '0') {
        $badServerFingerprints[$algo] = '1' . substr($fingerprint, 1);
    } else {
        $badServerFingerprints[$algo] = '0' . substr($fingerprint, 1);
    }
}
$certGenerator->saveNewCertAsFileWithKey(
    'client',
    __DIR__ . '/tls_peer_fingerprint_client.pem',
    'prime256v1',
    'IP:127.0.0.1,DNS:localhost',
    'critical,clientAuth'
);
$clientFingerprints = [];
foreach (['md5', 'sha1', 'sha256', 'sha512', 'sha3-256', 'sm3', 'blake2b512'] as $algo) {
    if (in_array($algo, openssl_get_md_methods(), true)) {
        $clientFingerprints[$algo] = $certGenerator->getCertDigest($algo);
    }
}
$badClientFingerprints = [];
foreach ($clientFingerprints as $algo => $fingerprint) {
    $badClientFingerprints[$algo] = $fingerprint;
    if ($fingerprint[0] === '0') {
        $badClientFingerprints[$algo] = '1' . substr($fingerprint, 1);
    } else {
        $badClientFingerprints[$algo] = '0' . substr($fingerprint, 1);
    }
}
// seems sha1 and sha256 must be supported by openssl
// see https://github.com/openssl/openssl/blob/0755a8ef905800ebc4ee022f880119f3e67b64bc/crypto/evp/c_alld.c

$commonClientOptions = [
    'verify_peer' => true,
    'peer_name' => 'localhost',
    'cafile' => __DIR__ . '/tls_peer_fingerprint_ca.crt',
    'local_cert' => __DIR__ . '/tls_peer_fingerprint_client.pem',
    'local_pk' => __DIR__ . '/tls_peer_fingerprint_client.pem',
];
$commonServerOptions = [
    'verify_peer' => false,
    'cafile' => __DIR__ . '/tls_peer_fingerprint_ca.crt',
    'local_cert' => __DIR__ . '/tls_peer_fingerprint_server.pem',
    'local_pk' => __DIR__ . '/tls_peer_fingerprint_server.pem',
];

// php will only show warnings, so convert warnings to exceptions
class PHPWarningException extends Exception
{
}
set_error_handler(static function (int $errno, string $errstr): void {
    throw new PHPWarningException($errstr);
});

// 0: bad arguments

echo '0a. bad string fingerprint length' . PHP_EOL;
$clientcontext = stream_context_create([
    'ssl' => [
        ...$commonClientOptions,
        'peer_fingerprint' => 'cafebabe',
    ],
]);
Assert::throws(static function () use ($clientcontext): void {
    file_get_contents('https://www.baidu.com', context: $clientcontext);
}, PHPWarningException::class, expectMessage: '/Unknown digest algorithm/');

echo '0b. bad md5 string fingerprint value' . PHP_EOL;
$clientcontext = stream_context_create([
    'ssl' => [
        ...$commonClientOptions,
        'peer_fingerprint' => 'notadigestcafebabecafebabecafeba',
    ],
]);
Assert::throws(static function () use ($clientcontext): void {
    file_get_contents('https://www.baidu.com', context: $clientcontext);
}, PHPWarningException::class, expectMessage: '/peer_fingerprint match failure/');

echo '0c. bad sha1 string fingerprint value' . PHP_EOL;
$clientcontext = stream_context_create([
    'ssl' => [
        ...$commonClientOptions,
        'peer_fingerprint' => 'notadigestcafebabecafebabecafebabecafeba',
    ],
]);
Assert::throws(static function () use ($clientcontext): void {
    file_get_contents('https://www.baidu.com', context: $clientcontext);
}, PHPWarningException::class, expectMessage: '/peer_fingerprint match failure/');

echo '0d. empty array fingerprint' . PHP_EOL;
$clientcontext = stream_context_create([
    'ssl' => [
        ...$commonClientOptions,
        'peer_fingerprint' => [],
    ],
]);
Assert::throws(static function () use ($clientcontext): void {
    file_get_contents('https://www.baidu.com', context: $clientcontext);
}, PHPWarningException::class, expectMessage: '/Invalid peer_fingerprint array; \[algo => fingerprint\] form required/');

echo '0e. bad array format' . PHP_EOL;
$clientcontext = stream_context_create([
    'ssl' => [
        ...$commonClientOptions,
        'peer_fingerprint' => [
            'cafebabedeadbeefcafebabedeadbeef',
        ],
    ],
]);
Assert::throws(static function () use ($clientcontext): void {
    file_get_contents('https://www.baidu.com', context: $clientcontext);
}, PHPWarningException::class, expectMessage: '/Invalid peer_fingerprint array; \[algo => fingerprint\] form required/');

echo '0f. bad array value' . PHP_EOL;
$clientcontext = stream_context_create([
    'ssl' => [
        ...$commonClientOptions,
        'peer_fingerprint' => [
            'md5' => 'notadigestcafebabedeadbeefcafeba',
        ],
    ],
]);
Assert::throws(static function () use ($clientcontext): void {
    file_get_contents('https://www.baidu.com', context: $clientcontext);
}, PHPWarningException::class, expectMessage: '/peer_fingerprint match failure/');

echo '0g. bad array value' . PHP_EOL;
$clientcontext = stream_context_create([
    'ssl' => [
        ...$commonClientOptions,
        'peer_fingerprint' => [
            'md5' => '123',
        ],
    ],
]);
Assert::throws(static function () use ($clientcontext): void {
    file_get_contents('https://www.baidu.com', context: $clientcontext);
}, PHPWarningException::class, expectMessage: '/peer_fingerprint match failure/');

echo '0h. bad array value' . PHP_EOL;
$clientcontext = stream_context_create([
    'ssl' => [
        ...$commonClientOptions,
        'peer_fingerprint' => [
            'md5' => '1234',
        ],
    ],
]);
Assert::throws(static function () use ($clientcontext): void {
    file_get_contents('https://www.baidu.com', context: $clientcontext);
}, PHPWarningException::class, expectMessage: '/peer_fingerprint match failure/');

echo '0i. bad method' . PHP_EOL;
if (!in_array('不会真有人叫这个吧', openssl_get_md_methods(), true)) {
    $clientcontext = stream_context_create([
        'ssl' => [
            ...$commonClientOptions,
            'peer_fingerprint' => [
                '不会真有人叫这个吧' => '1234',
            ],
        ],
    ]);
    Assert::throws(static function () use ($clientcontext): void {
        file_get_contents('https://www.baidu.com', context: $clientcontext);
    }, PHPWarningException::class, expectMessage: '/Unknown digest algorithm/');
}

echo 'Done' . PHP_EOL;
?>
--CLEAN--
<?php
@unlink(__DIR__ . '/tls_peer_fingerprint_ca.crt');
@unlink(__DIR__ . '/tls_peer_fingerprint_server.pem');
@unlink(__DIR__ . '/tls_peer_fingerprint_client.pem');
?>
--EXPECT--
0a. bad string fingerprint length
0b. bad md5 string fingerprint value
0c. bad sha1 string fingerprint value
0d. empty array fingerprint
0e. bad array format
0f. bad array value
0g. bad array value
0h. bad array value
0i. bad method
Done
