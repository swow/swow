--TEST--
swow_socket: tls peer fingerprint
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

$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1')->listen();

$commonClientOptions = [
    'verify_peer' => true,
    'peer_name' => 'localhost',
    'ca_file' => __DIR__ . '/tls_peer_fingerprint_ca.crt',
    'certificate' => __DIR__ . '/tls_peer_fingerprint_client.pem',
    'certificate_key' => __DIR__ . '/tls_peer_fingerprint_client.pem',
];
$commonServerOptions = [
    'verify_peer' => false,
    'ca_file' => __DIR__ . '/tls_peer_fingerprint_ca.crt',
    'certificate' => __DIR__ . '/tls_peer_fingerprint_server.pem',
    'certificate_key' => __DIR__ . '/tls_peer_fingerprint_server.pem',
];

// 0: bad arguments

echo '0a. bad string fingerprint length' . PHP_EOL;
$client = new Socket(Socket::TYPE_TCP);
$conn = $client->connect($server->getSockAddress(), $server->getSockPort());
Assert::throws(static function () use ($conn, $commonClientOptions): void {
    $conn->enableCrypto([
        ...$commonClientOptions,
        'peer_fingerprint' => 'cafebabe',
    ]);
}, 'Swow\SocketException', expectMessage: '/invalid peer fingerprint length.+/');

echo '0b. bad md5 string fingerprint value' . PHP_EOL;
$client = new Socket(Socket::TYPE_TCP);
$conn = $client->connect($server->getSockAddress(), $server->getSockPort());
Assert::throws(static function () use ($conn, $commonClientOptions): void {
    $conn->enableCrypto([
        ...$commonClientOptions,
        'peer_fingerprint' => 'notadigestcafebabecafebabecafeba',
    ]);
}, 'Swow\SocketException', expectMessage: '/invalid peer fingerprint, expected hex string/');

echo '0c. bad sha1 string fingerprint value' . PHP_EOL;
$client = new Socket(Socket::TYPE_TCP);
$conn = $client->connect($server->getSockAddress(), $server->getSockPort());
Assert::throws(static function () use ($conn, $commonClientOptions): void {
    $conn->enableCrypto([
        ...$commonClientOptions,
        'peer_fingerprint' => 'notadigestcafebabecafebabecafebabecafeba',
    ]);
}, 'Swow\SocketException', expectMessage: '/invalid peer fingerprint, expected hex string/');

echo '0d. empty array fingerprint' . PHP_EOL;
$client = new Socket(Socket::TYPE_TCP);
$conn = $client->connect($server->getSockAddress(), $server->getSockPort());
Assert::throws(static function () use ($conn, $commonClientOptions): void {
    $conn->enableCrypto([
        ...$commonClientOptions,
        'peer_fingerprint' => [],
    ]);
}, 'Swow\SocketException', expectMessage: '/invalid peer fingerprint, expected array with at least one element/');

echo '0e. bad array format' . PHP_EOL;
$client = new Socket(Socket::TYPE_TCP);
$conn = $client->connect($server->getSockAddress(), $server->getSockPort());
Assert::throws(static function () use ($conn, $commonClientOptions): void {
    $conn->enableCrypto([
        ...$commonClientOptions,
        'peer_fingerprint' => [
            'cafebabedeadbeefcafebabedeadbeef',
        ],
    ]);
}, 'Swow\SocketException', expectMessage: '/invalid peer fingerprint, expected array with string key and hex string value/');

echo '0f. bad array value' . PHP_EOL;
$client = new Socket(Socket::TYPE_TCP);
$conn = $client->connect($server->getSockAddress(), $server->getSockPort());
Assert::throws(static function () use ($conn, $commonClientOptions): void {
    $conn->enableCrypto([
        ...$commonClientOptions,
        'peer_fingerprint' => [
            'md5' => 'notadigestcafebabedeadbeefcafeba',
        ],
    ]);
}, 'Swow\SocketException', expectMessage: '/invalid peer fingerprint value, expected hex string/');

echo '0g. bad array value' . PHP_EOL;
$client = new Socket(Socket::TYPE_TCP);
$conn = $client->connect($server->getSockAddress(), $server->getSockPort());
Assert::throws(static function () use ($conn, $commonClientOptions): void {
    $conn->enableCrypto([
        ...$commonClientOptions,
        'peer_fingerprint' => [
            'md5' => '123',
        ],
    ]);
}, 'Swow\SocketException', expectMessage: '/invalid peer fingerprint hex string length/');

echo '0h. bad array value' . PHP_EOL;
$client = new Socket(Socket::TYPE_TCP);
$conn = $client->connect($server->getSockAddress(), $server->getSockPort());
Assert::throws(static function () use ($conn, $commonClientOptions): void {
    $conn->enableCrypto([
        ...$commonClientOptions,
        'peer_fingerprint' => [
            'md5' => '1234',
        ],
    ]);
}, 'Swow\SocketException', expectMessage: '/invalid peer fingerprint hex string length/');

echo '0i. bad method' . PHP_EOL;
if (!in_array('不会真有人叫这个吧', openssl_get_md_methods(), true)) {
    $client = new Socket(Socket::TYPE_TCP);
    $conn = $client->connect($server->getSockAddress(), $server->getSockPort());
    Assert::throws(static function () use ($conn, $commonClientOptions): void {
        $conn->enableCrypto([
            ...$commonClientOptions,
            'peer_fingerprint' => [
                '不会真有人叫这个吧' => '1234',
            ],
        ]);
    }, 'Swow\SocketException', expectMessage: '/invalid peer fingerprint algorithm/');
}

echo '0j. without peer fingerprint check, should success' . PHP_EOL;
$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1')->listen();
Coroutine::run(static function () use ($server, $commonServerOptions): void {
    $conn = $server->accept();
    $conn->enableCrypto([
        ...$commonServerOptions,
    ]);
    $conn->send('helloFromServer');
    $data = $conn->recvString();
    Assert::same('helloFromClient', $data);
    $conn->close();
});
$client = new Socket(Socket::TYPE_TCP);
$conn = $client->connect($server->getSockAddress(), $server->getSockPort());
$conn->enableCrypto([
    ...$commonClientOptions,
]);
$data = $conn->recvString();
Assert::same('helloFromServer', $data);
$conn->send('helloFromClient');
$conn->close();

// 1: client checks server fingerprint

if (isset($serverFingerprints['md5'])) {
    echo '1a. client checks server fingerprint with single md5 string, success' . PHP_EOL;
    $server = new Socket(Socket::TYPE_TCP);
    $server->bind('127.0.0.1')->listen();
    Coroutine::run(static function () use ($server, $commonServerOptions): void {
        $conn = $server->accept();
        $conn->enableCrypto([
            ...$commonServerOptions,
        ]);
        $conn->send('helloFromServer');
        $data = $conn->recvString();
        Assert::same('helloFromClient', $data);
        $conn->close();
    });
    $client = new Socket(Socket::TYPE_TCP);
    $conn = $client->connect($server->getSockAddress(), $server->getSockPort());
    $conn->enableCrypto([
        ...$commonClientOptions,
        'peer_fingerprint' => $serverFingerprints['md5'],
    ]);
    $data = $conn->recvString();
    Assert::same('helloFromServer', $data);
    $conn->send('helloFromClient');
    $conn->close();

    echo '1b. client checks server fingerprint with single md5 string, failure' . PHP_EOL;
    $server = new Socket(Socket::TYPE_TCP);
    $server->bind('127.0.0.1')->listen();
    Coroutine::run(static function () use ($server, $commonServerOptions): void {
        $conn = $server->accept();
        Assert::throws(static function () use ($conn, $commonServerOptions): void {
            $conn->enableCrypto([
                ...$commonServerOptions,
            ]);
        }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');
    });
    $client = new Socket(Socket::TYPE_TCP);
    $conn = $client->connect($server->getSockAddress(), $server->getSockPort());
    Assert::throws(static function () use ($conn, $commonClientOptions, $badServerFingerprints): void {
        $conn->enableCrypto([
            ...$commonClientOptions,
            'peer_fingerprint' => $badServerFingerprints['md5'],
        ]);
    }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');

    echo '1c. client checks server fingerprint with md5 array, success' . PHP_EOL;
    $server = new Socket(Socket::TYPE_TCP);
    $server->bind('127.0.0.1')->listen();
    Coroutine::run(static function () use ($server, $commonServerOptions): void {
        $conn = $server->accept();
        $conn->enableCrypto([
            ...$commonServerOptions,
        ]);
        $conn->send('helloFromServer');
        $data = $conn->recvString();
        Assert::same('helloFromClient', $data);
        $conn->close();
    });
    $client = new Socket(Socket::TYPE_TCP);
    $conn = $client->connect($server->getSockAddress(), $server->getSockPort());
    $conn->enableCrypto([
        ...$commonClientOptions,
        'peer_fingerprint' => [
            'md5' => $serverFingerprints['md5'],
        ],
    ]);
    $data = $conn->recvString();
    Assert::same('helloFromServer', $data);
    $conn->send('helloFromClient');
    $conn->close();

    echo '1d. client checks server fingerprint with md5 array, failure' . PHP_EOL;
    $server = new Socket(Socket::TYPE_TCP);
    $server->bind('127.0.0.1')->listen();
    Coroutine::run(static function () use ($server, $commonServerOptions): void {
        $conn = $server->accept();
        Assert::throws(static function () use ($conn, $commonServerOptions): void {
            $conn->enableCrypto([
                ...$commonServerOptions,
            ]);
        }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');
    });
    $client = new Socket(Socket::TYPE_TCP);
    $conn = $client->connect($server->getSockAddress(), $server->getSockPort());
    Assert::throws(static function () use ($conn, $commonClientOptions, $badServerFingerprints): void {
        $conn->enableCrypto([
            ...$commonClientOptions,
            'peer_fingerprint' => [
                'md5' => $badServerFingerprints['md5'],
            ],
        ]);
    }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');
}

echo '1e. client checks server fingerprint with single sha1 string, success' . PHP_EOL;
$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1')->listen();
Coroutine::run(static function () use ($server, $commonServerOptions): void {
    $conn = $server->accept();
    $conn->enableCrypto([
        ...$commonServerOptions,
    ]);
    $conn->send('helloFromServer');
    $data = $conn->recvString();
    Assert::same('helloFromClient', $data);
    $conn->close();
});
$client = new Socket(Socket::TYPE_TCP);
$conn = $client->connect($server->getSockAddress(), $server->getSockPort());
$conn->enableCrypto([
    ...$commonClientOptions,
    'peer_fingerprint' => $serverFingerprints['sha1'],
]);
$data = $conn->recvString();
Assert::same('helloFromServer', $data);
$conn->send('helloFromClient');
$conn->close();

echo '1f. client checks server fingerprint with single sha1 string, failure' . PHP_EOL;
$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1')->listen();
Coroutine::run(static function () use ($server, $commonServerOptions): void {
    $conn = $server->accept();
    Assert::throws(static function () use ($conn, $commonServerOptions): void {
        $conn->enableCrypto([
            ...$commonServerOptions,
        ]);
    }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');
});
$client = new Socket(Socket::TYPE_TCP);
$conn = $client->connect($server->getSockAddress(), $server->getSockPort());
Assert::throws(static function () use ($conn, $commonClientOptions, $badServerFingerprints): void {
    $conn->enableCrypto([
        ...$commonClientOptions,
        'peer_fingerprint' => $badServerFingerprints['sha1'],
    ]);
}, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');

echo '1g. client checks server fingerprint with sha1 array, success' . PHP_EOL;
$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1')->listen();
Coroutine::run(static function () use ($server, $commonServerOptions): void {
    $conn = $server->accept();
    $conn->enableCrypto([
        ...$commonServerOptions,
    ]);
    $conn->send('helloFromServer');
    $data = $conn->recvString();
    Assert::same('helloFromClient', $data);
    $conn->close();
});
$client = new Socket(Socket::TYPE_TCP);
$conn = $client->connect($server->getSockAddress(), $server->getSockPort());
$conn->enableCrypto([
    ...$commonClientOptions,
    'peer_fingerprint' => [
        'sha1' => $serverFingerprints['sha1'],
    ],
]);
$data = $conn->recvString();
Assert::same('helloFromServer', $data);
$conn->send('helloFromClient');
$conn->close();

echo '1h. client checks server fingerprint with sha1 array, failure' . PHP_EOL;
$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1')->listen();
Coroutine::run(static function () use ($server, $commonServerOptions): void {
    $conn = $server->accept();
    Assert::throws(static function () use ($conn, $commonServerOptions): void {
        $conn->enableCrypto([
            ...$commonServerOptions,
        ]);
    }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');
});
$client = new Socket(Socket::TYPE_TCP);
$conn = $client->connect($server->getSockAddress(), $server->getSockPort());
Assert::throws(static function () use ($conn, $commonClientOptions, $badServerFingerprints): void {
    $conn->enableCrypto([
        ...$commonClientOptions,
        'peer_fingerprint' => [
            'sha1' => $badServerFingerprints['sha1'],
        ],
    ]);
}, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');

echo '1i. client checks server fingerprint with sha1 and sha256 array, success' . PHP_EOL;
$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1')->listen();
Coroutine::run(static function () use ($server, $commonServerOptions): void {
    $conn = $server->accept();
    $conn->enableCrypto([
        ...$commonServerOptions,
    ]);
    $conn->send('helloFromServer');
    $data = $conn->recvString();
    Assert::same('helloFromClient', $data);
    $conn->close();
});
$client = new Socket(Socket::TYPE_TCP);
$conn = $client->connect($server->getSockAddress(), $server->getSockPort());
$conn->enableCrypto([
    ...$commonClientOptions,
    'peer_fingerprint' => [
        'sha1' => $serverFingerprints['sha1'],
        'sha256' => $serverFingerprints['sha256'],
    ],
]);
$data = $conn->recvString();
Assert::same('helloFromServer', $data);
$conn->send('helloFromClient');
$conn->close();

echo '1j. client checks server fingerprint with sha1 and sha256 array, failure' . PHP_EOL;
$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1')->listen();
Coroutine::run(static function () use ($server, $commonServerOptions): void {
    $conn = $server->accept();
    Assert::throws(static function () use ($conn, $commonServerOptions): void {
        $conn->enableCrypto([
            ...$commonServerOptions,
        ]);
    }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');
});
$client = new Socket(Socket::TYPE_TCP);
$conn = $client->connect($server->getSockAddress(), $server->getSockPort());
Assert::throws(static function () use ($conn, $commonClientOptions, $badServerFingerprints): void {
    $conn->enableCrypto([
        ...$commonClientOptions,
        'peer_fingerprint' => [
            'sha1' => $badServerFingerprints['sha1'],
            'sha256' => $badServerFingerprints['sha256'],
        ],
    ]);
}, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');

echo '1k. client checks server fingerprint with sha1 and sha256 array, failure' . PHP_EOL;
$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1')->listen();
Coroutine::run(static function () use ($server, $commonServerOptions): void {
    $conn = $server->accept();
    Assert::throws(static function () use ($conn, $commonServerOptions): void {
        $conn->enableCrypto([
            ...$commonServerOptions,
        ]);
    }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');
});
$client = new Socket(Socket::TYPE_TCP);
$conn = $client->connect($server->getSockAddress(), $server->getSockPort());
Assert::throws(static function () use ($conn, $commonClientOptions, $serverFingerprints, $badServerFingerprints): void {
    $conn->enableCrypto([
        ...$commonClientOptions,
        'peer_fingerprint' => [
            'sha1' => $serverFingerprints['sha1'],
            'sha256' => $badServerFingerprints['sha256'],
        ],
    ]);
}, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');

// 2: server checks client fingerprint
$commonServerOptions = [
    'verify_peer' => true,
    'peer_name' => 'localhost',
    'ca_file' => __DIR__ . '/tls_peer_fingerprint_ca.crt',
    'certificate' => __DIR__ . '/tls_peer_fingerprint_server.pem',
    'certificate_key' => __DIR__ . '/tls_peer_fingerprint_server.pem',
];

if (isset($serverFingerprints['md5'])) {
    echo '2a. server checks client fingerprint with single md5 string, success' . PHP_EOL;
    $server = new Socket(Socket::TYPE_TCP);
    $server->bind('127.0.0.1')->listen();
    Coroutine::run(static function () use ($server, $commonServerOptions, $clientFingerprints): void {
        $conn = $server->accept();
        $conn->enableCrypto([
            ...$commonServerOptions,
            'peer_fingerprint' => $clientFingerprints['md5'],
        ]);
        $conn->send('helloFromServer');
        $data = $conn->recvString();
        Assert::same('helloFromClient', $data);
        $conn->close();
    });
    $client = new Socket(Socket::TYPE_TCP);
    $conn = $client->connect($server->getSockAddress(), $server->getSockPort());
    $conn->enableCrypto([
        ...$commonClientOptions,
    ]);
    $data = $conn->recvString();
    Assert::same('helloFromServer', $data);
    $conn->send('helloFromClient');
    $conn->close();

    echo '2b. server checks client fingerprint with single md5 string, failure' . PHP_EOL;
    $server = new Socket(Socket::TYPE_TCP);
    $server->bind('127.0.0.1')->listen();
    Coroutine::run(static function () use ($server, $commonServerOptions, $badClientFingerprints): void {
        $conn = $server->accept();
        Assert::throws(static function () use ($conn, $commonServerOptions, $badClientFingerprints): void {
            $conn->enableCrypto([
                ...$commonServerOptions,
                'peer_fingerprint' => $badClientFingerprints['md5'],
            ]);
        }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');
    });
    $client = new Socket(Socket::TYPE_TCP);
    $conn = $client->connect($server->getSockAddress(), $server->getSockPort());
    // quirk here: client will success, but server connection will be closed
    // Assert::throws(static function () use ($conn, $commonClientOptions): void {
    $conn->enableCrypto([
        ...$commonClientOptions,
    ]);
    // }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');

    echo '2c. server checks client fingerprint with md5 array, success' . PHP_EOL;
    $server = new Socket(Socket::TYPE_TCP);
    $server->bind('127.0.0.1')->listen();
    Coroutine::run(static function () use ($server, $commonServerOptions, $clientFingerprints): void {
        $conn = $server->accept();
        $conn->enableCrypto([
            ...$commonServerOptions,
            'peer_fingerprint' => [
                'md5' => $clientFingerprints['md5'],
            ],
        ]);
        $conn->send('helloFromServer');
        $data = $conn->recvString();
        Assert::same('helloFromClient', $data);
        $conn->close();
    });
    $client = new Socket(Socket::TYPE_TCP);
    $conn = $client->connect($server->getSockAddress(), $server->getSockPort());
    $conn->enableCrypto([
        ...$commonClientOptions,
    ]);
    $data = $conn->recvString();
    Assert::same('helloFromServer', $data);
    $conn->send('helloFromClient');
    $conn->close();

    echo '2d. server checks client fingerprint with md5 array, failure' . PHP_EOL;
    $server = new Socket(Socket::TYPE_TCP);
    $server->bind('127.0.0.1')->listen();
    Coroutine::run(static function () use ($server, $commonServerOptions, $badClientFingerprints): void {
        $conn = $server->accept();
        Assert::throws(static function () use ($conn, $commonServerOptions, $badClientFingerprints): void {
            $conn->enableCrypto([
                ...$commonServerOptions,
                'peer_fingerprint' => [
                    'md5' => $badClientFingerprints['md5'],
                ],
            ]);
        }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');
    });
    $client = new Socket(Socket::TYPE_TCP);
    $conn = $client->connect($server->getSockAddress(), $server->getSockPort());
    // quirk here: client will success, but server connection will be closed
    // Assert::throws(static function () use ($conn, $commonClientOptions): void {
    $conn->enableCrypto([
        ...$commonClientOptions,
    ]);
// }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');
} else {
    // echo only
    echo '2a. server checks client fingerprint with single md5 string, success' . PHP_EOL;
    echo '2b. server checks client fingerprint with single md5 string, failure' . PHP_EOL;
    echo '2c. server checks client fingerprint with md5 array, success' . PHP_EOL;
    echo '2d. server checks client fingerprint with md5 array, failure' . PHP_EOL;
}

echo '2e. server checks client fingerprint with single sha1 string, success' . PHP_EOL;
$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1')->listen();
Coroutine::run(static function () use ($server, $commonServerOptions, $clientFingerprints): void {
    $conn = $server->accept();
    $conn->enableCrypto([
        ...$commonServerOptions,
        'peer_fingerprint' => $clientFingerprints['sha1'],
    ]);
    $conn->send('helloFromServer');
    $data = $conn->recvString();
    Assert::same('helloFromClient', $data);
    $conn->close();
});
$client = new Socket(Socket::TYPE_TCP);
$conn = $client->connect($server->getSockAddress(), $server->getSockPort());
$conn->enableCrypto([
    ...$commonClientOptions,
]);
$data = $conn->recvString();
Assert::same('helloFromServer', $data);
$conn->send('helloFromClient');
$conn->close();

echo '2f. server checks client fingerprint with single sha1 string, failure' . PHP_EOL;
$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1')->listen();
Coroutine::run(static function () use ($server, $commonServerOptions, $badClientFingerprints): void {
    $conn = $server->accept();
    Assert::throws(static function () use ($conn, $commonServerOptions, $badClientFingerprints): void {
        $conn->enableCrypto([
            ...$commonServerOptions,
            'peer_fingerprint' => $badClientFingerprints['sha1'],
        ]);
    }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');
});
$client = new Socket(Socket::TYPE_TCP);
$conn = $client->connect($server->getSockAddress(), $server->getSockPort());
// quirk here: client will success, but server connection will be closed
// Assert::throws(static function () use ($conn, $commonClientOptions): void {
$conn->enableCrypto([
    ...$commonClientOptions,
]);
// }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');

echo '2g. server checks client fingerprint with sha1 array, success' . PHP_EOL;
$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1')->listen();
Coroutine::run(static function () use ($server, $commonServerOptions, $clientFingerprints): void {
    $conn = $server->accept();
    $conn->enableCrypto([
        ...$commonServerOptions,
        'peer_fingerprint' => [
            'sha1' => $clientFingerprints['sha1'],
        ],
    ]);
    $conn->send('helloFromServer');
    $data = $conn->recvString();
    Assert::same('helloFromClient', $data);
    $conn->close();
});
$client = new Socket(Socket::TYPE_TCP);
$conn = $client->connect($server->getSockAddress(), $server->getSockPort());
$conn->enableCrypto([
    ...$commonClientOptions,
]);
$data = $conn->recvString();
Assert::same('helloFromServer', $data);
$conn->send('helloFromClient');
$conn->close();

echo '2h. server checks client fingerprint with sha1 array, failure' . PHP_EOL;
$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1')->listen();
Coroutine::run(static function () use ($server, $commonServerOptions, $badClientFingerprints): void {
    $conn = $server->accept();
    Assert::throws(static function () use ($conn, $commonServerOptions, $badClientFingerprints): void {
        $conn->enableCrypto([
            ...$commonServerOptions,
            'peer_fingerprint' => [
                'sha1' => $badClientFingerprints['sha1'],
            ],
        ]);
    }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');
});
$client = new Socket(Socket::TYPE_TCP);
$conn = $client->connect($server->getSockAddress(), $server->getSockPort());
// quirk here: client will success, but server connection will be closed
// Assert::throws(static function () use ($conn, $commonClientOptions): void {
$conn->enableCrypto([
    ...$commonClientOptions,
]);
// }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');

echo '2i. server checks client fingerprint with sha1 and sha256 array, success' . PHP_EOL;
$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1')->listen();
Coroutine::run(static function () use ($server, $commonServerOptions, $clientFingerprints): void {
    $conn = $server->accept();
    $conn->enableCrypto([
        ...$commonServerOptions,
        'peer_fingerprint' => [
            'sha1' => $clientFingerprints['sha1'],
            'sha256' => $clientFingerprints['sha256'],
        ],
    ]);
    $conn->send('helloFromServer');
    $data = $conn->recvString();
    Assert::same('helloFromClient', $data);
    $conn->close();
});
$client = new Socket(Socket::TYPE_TCP);
$conn = $client->connect($server->getSockAddress(), $server->getSockPort());
$conn->enableCrypto([
    ...$commonClientOptions,
]);
$data = $conn->recvString();
Assert::same('helloFromServer', $data);
$conn->send('helloFromClient');
$conn->close();

echo '2j. server checks client fingerprint with sha1 and sha256 array, failure' . PHP_EOL;
$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1')->listen();
Coroutine::run(static function () use ($server, $commonServerOptions, $badClientFingerprints): void {
    $conn = $server->accept();
    Assert::throws(static function () use ($conn, $commonServerOptions, $badClientFingerprints): void {
        $conn->enableCrypto([
            ...$commonServerOptions,
            'peer_fingerprint' => [
                'sha1' => $badClientFingerprints['sha1'],
                'sha256' => $badClientFingerprints['sha256'],
            ],
        ]);
    }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');
});
$client = new Socket(Socket::TYPE_TCP);
$conn = $client->connect($server->getSockAddress(), $server->getSockPort());
// quirk here: client will success, but server connection will be closed
// Assert::throws(static function () use ($conn, $commonClientOptions): void {
$conn->enableCrypto([
    ...$commonClientOptions,
]);
// }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');

echo '2k. server checks client fingerprint with sha1 and sha256 array, failure' . PHP_EOL;
$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1')->listen();
Coroutine::run(static function () use ($server, $commonServerOptions, $clientFingerprints, $badClientFingerprints): void {
    $conn = $server->accept();
    Assert::throws(static function () use ($conn, $commonServerOptions, $clientFingerprints, $badClientFingerprints): void {
        $conn->enableCrypto([
            ...$commonServerOptions,
            'peer_fingerprint' => [
                'sha1' => $clientFingerprints['sha1'],
                'sha256' => $badClientFingerprints['sha256'],
            ],
        ]);
    }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');
});
$client = new Socket(Socket::TYPE_TCP);
$conn = $client->connect($server->getSockAddress(), $server->getSockPort());
// quirk here: client will success, but server connection will be closed
// Assert::throws(static function () use ($conn, $commonClientOptions): void {
$conn->enableCrypto([
    ...$commonClientOptions,
]);
// }, 'Swow\SocketException', expectMessage: '/Socket enable crypto failed/');

echo "Done\n";
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
0j. without peer fingerprint check, should success
1a. client checks server fingerprint with single md5 string, success
1b. client checks server fingerprint with single md5 string, failure
1c. client checks server fingerprint with md5 array, success
1d. client checks server fingerprint with md5 array, failure
1e. client checks server fingerprint with single sha1 string, success
1f. client checks server fingerprint with single sha1 string, failure
1g. client checks server fingerprint with sha1 array, success
1h. client checks server fingerprint with sha1 array, failure
1i. client checks server fingerprint with sha1 and sha256 array, success
1j. client checks server fingerprint with sha1 and sha256 array, failure
1k. client checks server fingerprint with sha1 and sha256 array, failure
2a. server checks client fingerprint with single md5 string, success
2b. server checks client fingerprint with single md5 string, failure
2c. server checks client fingerprint with md5 array, success
2d. server checks client fingerprint with md5 array, failure
2e. server checks client fingerprint with single sha1 string, success
2f. server checks client fingerprint with single sha1 string, failure
2g. server checks client fingerprint with sha1 array, success
2h. server checks client fingerprint with sha1 array, failure
2i. server checks client fingerprint with sha1 and sha256 array, success
2j. server checks client fingerprint with sha1 and sha256 array, failure
2k. server checks client fingerprint with sha1 and sha256 array, failure
Done
