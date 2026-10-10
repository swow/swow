--TEST--
swow_socket: socket and stream fingerprint values reject invalid types and accept references
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!extension_loaded('openssl'), 'openssl extension is required');
skip_if(!Swow\Extension::isBuiltWith('openssl'), 'extension must be built with ssl');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Socket;
use Swow\SocketException;

$generator = new CertificateGenerator(false);
$certificate = tempnam(sys_get_temp_dir(), 'swow_fingerprint');
$ca = tempnam(sys_get_temp_dir(), 'swow_fingerprint_ca');
$generator->saveCaCert($ca);
$generator->saveNewCertAsFileWithKey('localhost', $certificate, 'prime256v1', 'DNS:localhost,IP:127.0.0.1', 'critical,serverAuth');
$fingerprint = $generator->getCertDigest('sha256');
[$server, $coroutine] = createHttpTestServer($certificate);
$client = null;
try {
    foreach ([null, false, true, 42, 1.5, [], new stdClass()] as $value) {
        $client = (new Socket(Socket::TYPE_TCP))->setTimeout(1000)->connect('127.0.0.1', $server->getSockPort());
        Assert::throws(static function () use ($client, $value): void {
            $client->enableCrypto(['peer_fingerprint' => ['sha256' => $value]]);
        }, SocketException::class, expectMessage: '/expected array with string key and hex string value/');
        Assert::true($client->isEstablished());
        $fingerprints = ['sha256' => &$fingerprint];
        $options = [
            'verify_peer' => true,
            'verify_peer_name' => true,
            'peer_name' => 'localhost',
            'ca_file' => $ca,
            'peer_fingerprint' => &$fingerprints,
        ];
        $client->enableCrypto($options);
        Assert::same($options['peer_fingerprint']['sha256'], $fingerprint);
        $client->send("GET /fingerprint HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
        $response = '';
        while (($part = $client->recvString()) !== '') {
            $response .= $part;
        }
        Assert::contains($response, "HTTP/1.1 200 OK\r\n");
        Assert::contains($response, "\r\n\r\nDone");
        $client->close();
    }
    foreach ([null, false, true, 42, 1.5, [], new stdClass()] as $value) {
        $options = [
            'verify_peer' => true,
            'verify_peer_name' => true,
            'peer_name' => 'localhost',
            'cafile' => $ca,
            'peer_fingerprint' => ['sha256' => $value],
        ];
        set_error_handler(static function (int $severity, string $message): never {
            throw new ErrorException($message, 0, $severity);
        });
        try {
            Assert::throws(static function () use ($server, $options): void {
                stream_socket_client('tls://127.0.0.1:' . $server->getSockPort(), context: stream_context_create(['ssl' => $options]), timeout: 1);
            }, ErrorException::class, expectMessage: '/Invalid peer_fingerprint array/');
        } finally {
            restore_error_handler();
        }
        $options['peer_fingerprint'] = &$fingerprints;
        $stream = stream_socket_client('tls://127.0.0.1:' . $server->getSockPort(), context: stream_context_create(['ssl' => $options]), timeout: 1);
        try {
            stream_set_timeout($stream, 1);
            fwrite($stream, "GET /fingerprint HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
            $response = stream_get_contents($stream);
            Assert::contains($response, "HTTP/1.1 200 OK\r\n");
            Assert::contains($response, "\r\n\r\nDone");
        } finally {
            fclose($stream);
        }
    }
} finally {
    $client?->close();
    $server->close();
    if ($coroutine->isAlive()) {
        $coroutine->kill();
    }
    unlink($certificate);
    unlink($ca);
}
echo "Done\n";
?>
--EXPECT--
Done
