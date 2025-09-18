--TEST--
swow_socket: SSL ALPN
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!Swow\Extension::isBuiltWith('libcurl'), 'extension must be built with libcurl');
skip_if(!defined('CURL_HTTP_VERSION_2_0'), 'curl must be built with HTTP/2 support');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Sync\WaitReference;

$certFile = __DIR__ . DIRECTORY_SEPARATOR . 'ssl_alpn.pem.tmp';
$cacertFile = __DIR__ . DIRECTORY_SEPARATOR . 'ssl_alpn-ca.pem.tmp';

$certificateGenerator = new CertificateGenerator();
$certificateGenerator->saveCaCert($cacertFile);
$certificateGenerator->saveNewCertAsFileWithKey('ssl_alpn', $certFile);

$socket = new Swow\Socket(Swow\Socket::TYPE_TCP);
$server = stream_socket_server('tls://127.0.0.1:0', context: stream_context_create([
    'ssl' => [
        'alpn_protocols' => 'h2,http/1.1',
        'local_cert' => $certFile,
    ],
]));
$wr = new WaitReference();
Coroutine::run(static function () use ($server, $wr): void {
    $conn = stream_socket_accept($server);
    $payload = fread($conn, 1024);
    if (str_contains($payload, 'PRI * HTTP/2.0')) {
        echo "ALPN uses HTTP/2.0\n";
    } else {
        echo "ALPN uses HTTP/1.1\n";
    }
});

$serverName = stream_socket_get_name($server, false);
$serverUriParts = parse_url($serverName);
$serverHost = $serverUriParts['host'];
$serverPort = $serverUriParts['port'];

// use HTTP2 with -k
$ch = curl_init("https://{$serverHost}:{$serverPort}");
curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, false);
curl_setopt($ch, CURLOPT_SSL_VERIFYHOST, false);
curl_setopt($ch, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
curl_exec($ch);
if (PHP_VERSION_ID < 80100) {
    curl_close($ch);
}

$wr::wait($wr);

echo "Done\n";

?>
--EXPECT--
ALPN uses HTTP/2.0
Done
