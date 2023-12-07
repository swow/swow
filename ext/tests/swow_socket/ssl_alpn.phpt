--TEST--
swow_socket: SSL ALPN
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!getenv('SWOW_HAVE_SSL') && !Swow\Extension::isBuiltWith('ssl'), 'extension must be built with libcurl');
skip_if(!defined('CURL_HTTP_VERSION_2_0'), 'curl must be built with HTTP/2 support');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Sync\WaitReference;

$socket = new \Swow\Socket(Swow\Socket::TYPE_TCP);
$server = stream_socket_server('tls://127.0.0.1:0', context: stream_context_create([
    'ssl' => [
        'alpn_protocols' => 'h2,http/1.1',
        'local_cert' => __DIR__ . '/../include/ssl/server.crt',
        'local_pk' => __DIR__ . '/../include/ssl/server.key',
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
curl_close($ch);

$wr::wait($wr);

echo "Done\n";

?>
--EXPECT--
ALPN uses HTTP/2.0
Done
