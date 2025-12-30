--TEST--
swow_socket: tls SNI_server_certs
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
use Swow\Socket;
use Swow\SocketException;

$certGenerator = new CertificateGenerator(false);

$certGenerator->saveCaCert(__DIR__ . '/tls_sni_server_certs_ca.crt');
$certGenerator->saveNewCertAsFileWithKey(
    'localhost1',
    __DIR__ . '/tls_sni_server_certs_localhost1.pem',
    'prime256v1',
    'IP:127.0.0.1,DNS:localhost',
    'critical,serverAuth'
);
$certGenerator->saveNewCertAsFileWithKey(
    'localhost2',
    __DIR__ . '/tls_sni_server_certs_localhost2.pem',
    'prime256v1',
    'IP:127.0.0.1,DNS:test.some.local,DNS:test2.some.local',
    'critical,serverAuth'
);
$localhost2SHA1Fingerprint = $certGenerator->getCertDigest('sha1');
$certGenerator->saveNewCertAsFileWithKey(
    'localhost3',
    __DIR__ . '/tls_sni_server_certs_localhost3.pem',
    'prime256v1',
    'IP:127.0.0.1,DNS:*.some.local',
    'critical,serverAuth'
);
$localhost3SHA1Fingerprint = $certGenerator->getCertDigest('sha1');
$certGenerator->saveNewCertAsFileWithKey(
    'localhost',
    __DIR__ . '/tls_sni_server_certs_localhost4.pem',
    'prime256v1',
    null, // no SAN
    'critical,serverAuth'
);
$certGenerator->saveNewCertAsFileWithKey(
    'localhost5',
    __DIR__ . '/tls_sni_server_certs_localhost5.pem',
    'prime256v1',
    'IP:127.0.0.1,DNS:*.some.local',
    'critical,serverAuth'
);
$localhost5SHA1Fingerprint = $certGenerator->getCertDigest('sha1');
$certGenerator->saveNewCertAsFileWithKey(
    'notlocalhost',
    __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'prime256v1',
    'IP:127.0.0.1,DNS:notlocalhost',
    'critical,serverAuth'
);

$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1')->listen();

$commonClientOptions = [
    'verify_peer' => true,
    'verify_peer_name' => false,
    'SNI_enabled' => true,
    'ca_file' => __DIR__ . '/tls_sni_server_certs_ca.crt',
];
$commonServerOptions = [
    'verify_peer' => false,
    'peer_name' => 'localhost',
    'certificate' => __DIR__ . '/tls_sni_server_certs_localhost1.pem',
    'certificate_key' => __DIR__ . '/tls_sni_server_certs_localhost1.pem',
];

// 0: bad arguments
$tryConnect = static function ($serverName = 'localhost', $fingerprint = null) use ($server, $commonClientOptions): void {
    Coroutine::run(static function () use ($server, $commonClientOptions, $serverName, $fingerprint): void {
        $client = new Socket(Socket::TYPE_TCP);
        $client->connect($server->getSockAddress(), $server->getSockPort());
        try {
            $options = [
                ...$commonClientOptions,
                'peer_name' => $serverName,
                'verify_peer_name' => true,
            ];
            if ($fingerprint !== null) {
                $options['peer_fingerprint'] = [
                    'sha1' => $fingerprint,
                ];
            }
            $client->enableCrypto($options);
            $client->send('hello from client');
        } catch (SocketException $e) {
            // not care
        }
    });
};

echo '0a. bad SNI_server_certs type' . PHP_EOL;
$tryConnect();
Assert::throws(static function () use ($server, $commonServerOptions): void {
    $server->accept()->enableCrypto([
        ...$commonServerOptions,
        'SNI_server_certs' => 'not an array',
    ]);
}, 'Swow\SocketException', expectMessage: '/SNI_server_certs requires an array of configs/');

echo '0b. empty SNI_server_certs' . PHP_EOL;
$tryConnect();
Assert::throws(static function () use ($server, $commonServerOptions): void {
    $server->accept()->enableCrypto([
        ...$commonServerOptions,
        'SNI_server_certs' => [],
    ]);
}, 'Swow\SocketException', expectMessage: '/SNI_server_certs host cert array must not be empty/');

echo '0c. bad SNI_server_certs value type 1' . PHP_EOL;
$tryConnect();
Assert::throws(static function () use ($server, $commonServerOptions): void {
    $server->accept()->enableCrypto([
        ...$commonServerOptions,
        'SNI_server_certs' => ['localhost' => false],
    ]);
}, 'Swow\SocketException', expectMessage: '/SNI_server_certs value must be a string or array/');

echo '0d. bad SNI_server_certs value type 2' . PHP_EOL;
$tryConnect();
Assert::throws(static function () use ($server, $commonServerOptions): void {
    $server->accept()->enableCrypto([
        ...$commonServerOptions,
        'SNI_server_certs' => ['localhost' => [
            // no cert configurated
        ]],
    ]);
}, 'Swow\SocketException', expectMessage: '/certificate not present in SNI_server_certs/');

echo '0e. bad SNI_server_certs value type 3' . PHP_EOL;
$tryConnect();
Assert::throws(static function () use ($server, $commonServerOptions): void {
    $server->accept()->enableCrypto([
        ...$commonServerOptions,
        'SNI_server_certs' => ['localhost' => [
            'certificate' => __DIR__ . '/tls_sni_server_certs_localhost1.pem',
        ]],
    ]);
}, 'Swow\SocketException', expectMessage: '/certificate_key not present in SNI_server_certs/');

echo '0f. bad SNI_server_certs value type 4' . PHP_EOL;
$tryConnect();
Assert::throws(static function () use ($server, $commonServerOptions): void {
    $server->accept()->enableCrypto([
        ...$commonServerOptions,
        'SNI_server_certs' => [
            [
                // no san
                'certificate' => __DIR__ . '/tls_sni_server_certs_localhost4.pem',
                'certificate_key' => __DIR__ . '/tls_sni_server_certs_localhost4.pem',
            ],
        ],
    ]);
}, 'Swow\SocketException', expectMessage: '/No dns name SAN found for cert in SNI_server_certs/');

echo '0g. fail without SNI_server_certs' . PHP_EOL;
$tryConnect('localhost');
Assert::throws(static function () use ($server, $commonServerOptions): void {
    $server->accept()->enableCrypto([
        ...$commonServerOptions,
        'certificate' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
        'certificate_key' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    ]);
}, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');

// 1: valid SNI_server_certs
echo '1a. string type 1' . PHP_EOL;
$tryConnect('localhost');
$conn = $server->accept();
$conn->enableCrypto([
    ...$commonServerOptions,
    'certificate' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'certificate_key' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'SNI_server_certs' => [
        'localhost' => __DIR__ . '/tls_sni_server_certs_localhost1.pem',
    ],
]);
Assert::same('hello from client', $conn->recvString());

echo '1b. string type 2' . PHP_EOL;
$tryConnect('localhost');
$conn = $server->accept();
$conn->enableCrypto([
    ...$commonServerOptions,
    'certificate' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'certificate_key' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'SNI_server_certs' => [
        __DIR__ . '/tls_sni_server_certs_localhost1.pem',
    ],
]);
Assert::same('hello from client', $conn->recvString());

echo '1c. failback' . PHP_EOL;
$tryConnect('notlocalhost');
$conn = $server->accept();
$conn->enableCrypto([
    ...$commonServerOptions,
    'certificate' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'certificate_key' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'SNI_server_certs' => [
        'localhost' => __DIR__ . '/tls_sni_server_certs_localhost1.pem',
    ],
]);
Assert::same('hello from client', $conn->recvString());

echo '1d. array type 1' . PHP_EOL;
$tryConnect('localhost');
$conn = $server->accept();
$conn->enableCrypto([
    ...$commonServerOptions,
    'certificate' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'certificate_key' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'SNI_server_certs' => [
        'localhost' => [
            'certificate' => __DIR__ . '/tls_sni_server_certs_localhost1.pem',
            'certificate_key' => __DIR__ . '/tls_sni_server_certs_localhost1.pem',
        ],
    ],
]);
Assert::same('hello from client', $conn->recvString());

echo '1e. array type 2' . PHP_EOL;
$tryConnect('localhost');
$conn = $server->accept();
$conn->enableCrypto([
    ...$commonServerOptions,
    'certificate' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'certificate_key' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'SNI_server_certs' => [
        [
            'certificate' => __DIR__ . '/tls_sni_server_certs_localhost1.pem',
            'certificate_key' => __DIR__ . '/tls_sni_server_certs_localhost1.pem',
        ],
    ],
]);
Assert::same('hello from client', $conn->recvString());

echo '1f. mix type 1' . PHP_EOL;
$tryConnect('localhost');
$conn = $server->accept();
$conn->enableCrypto([
    ...$commonServerOptions,
    'certificate' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'certificate_key' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'SNI_server_certs' => [
        [
            'certificate' => __DIR__ . '/tls_sni_server_certs_localhost1.pem',
            'certificate_key' => __DIR__ . '/tls_sni_server_certs_localhost1.pem',
        ],
        __DIR__ . '/tls_sni_server_certs_localhost2.pem',
    ],
]);
Assert::same('hello from client', $conn->recvString());

echo '1g. mix type 2' . PHP_EOL;
$tryConnect('test.some.local');
$conn = $server->accept();
$conn->enableCrypto([
    ...$commonServerOptions,
    'certificate' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'certificate_key' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'SNI_server_certs' => [
        'localhost' => [
            'certificate' => __DIR__ . '/tls_sni_server_certs_localhost1.pem',
            'certificate_key' => __DIR__ . '/tls_sni_server_certs_localhost1.pem',
        ],
        __DIR__ . '/tls_sni_server_certs_localhost2.pem',
    ],
]);
Assert::same('hello from client', $conn->recvString());

echo '1h. wildcard 1' . PHP_EOL;
$tryConnect('test.some.local', $localhost2SHA1Fingerprint);
$conn = $server->accept();
$conn->enableCrypto([
    ...$commonServerOptions,
    'certificate' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'certificate_key' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'SNI_server_certs' => [
        // prefer full match, so localhost2 should be used
        __DIR__ . '/tls_sni_server_certs_localhost3.pem',
        __DIR__ . '/tls_sni_server_certs_localhost2.pem',
    ],
]);
Assert::same('hello from client', $conn->recvString());

echo '1i. wildcard 2' . PHP_EOL;
$tryConnect('test.some.local', $localhost2SHA1Fingerprint);
$conn = $server->accept();
$conn->enableCrypto([
    ...$commonServerOptions,
    'certificate' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'certificate_key' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'SNI_server_certs' => [
        // prefer full match, so localhost2 should be used
        __DIR__ . '/tls_sni_server_certs_localhost2.pem',
        __DIR__ . '/tls_sni_server_certs_localhost3.pem',
    ],
]);
Assert::same('hello from client', $conn->recvString());

echo '1j. wildcard 3' . PHP_EOL;
$tryConnect('randomstr.some.local');
$conn = $server->accept();
$conn->enableCrypto([
    ...$commonServerOptions,
    'certificate' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'certificate_key' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'SNI_server_certs' => [
        __DIR__ . '/tls_sni_server_certs_localhost3.pem',
    ],
]);
Assert::same('hello from client', $conn->recvString());

echo '1k. order 1' . PHP_EOL;
$tryConnect('randomstr.some.local', $localhost3SHA1Fingerprint);
$conn = $server->accept();
$conn->enableCrypto([
    ...$commonServerOptions,
    'certificate' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'certificate_key' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'SNI_server_certs' => [
        // all wildcard certs, follow the order
        __DIR__ . '/tls_sni_server_certs_localhost3.pem',
        __DIR__ . '/tls_sni_server_certs_localhost5.pem',
    ],
]);
Assert::same('hello from client', $conn->recvString());

echo '1l. order 2' . PHP_EOL;
$tryConnect('randomstr.some.local', $localhost5SHA1Fingerprint);
$conn = $server->accept();
$conn->enableCrypto([
    ...$commonServerOptions,
    'certificate' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'certificate_key' => __DIR__ . '/tls_sni_server_certs_notlocalhost.pem',
    'SNI_server_certs' => [
        // all wildcard certs, follow the order
        __DIR__ . '/tls_sni_server_certs_localhost5.pem',
        __DIR__ . '/tls_sni_server_certs_localhost3.pem',
    ],
]);
Assert::same('hello from client', $conn->recvString());

echo "Done\n";
?>
--CLEAN--
<?php
@unlink(__DIR__ . '/tls_sni_server_certs_ca.crt');
@unlink(__DIR__ . '/tls_sni_server_certs_localhost1.pem');
@unlink(__DIR__ . '/tls_sni_server_certs_localhost2.pem');
@unlink(__DIR__ . '/tls_sni_server_certs_localhost3.pem');
@unlink(__DIR__ . '/tls_sni_server_certs_localhost4.pem');
@unlink(__DIR__ . '/tls_sni_server_certs_localhost5.pem');
@unlink(__DIR__ . '/tls_sni_server_certs_notlocalhost.pem');
?>
--EXPECT--
0a. bad SNI_server_certs type
0b. empty SNI_server_certs
0c. bad SNI_server_certs value type 1
0d. bad SNI_server_certs value type 2
0e. bad SNI_server_certs value type 3
0f. bad SNI_server_certs value type 4
0g. fail without SNI_server_certs
1a. string type 1
1b. string type 2
1c. failback
1d. array type 1
1e. array type 2
1f. mix type 1
1g. mix type 2
1h. wildcard 1
1i. wildcard 2
1j. wildcard 3
1k. order 1
1l. order 2
Done
