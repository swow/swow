--TEST--
swow_curl: cancel
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(PHP_SAPI !== 'cli', 'only for cli');
skip_if(!Swow\Extension::isBuiltWith('curl'), 'extension must be built with libcurl');
skip_if(!Swow\Extension::isBuiltWith('openssl'), 'extension must be built with ssl');
skip_if(!extension_loaded('openssl'), 'openssl extension is required');
// curl is buggy on macOS
// this bug is fixed in github.com/curl/curl/commit/de3fc1d7adb78c078e4cc7ccc48e550758094ad3
skip_if(
    curl_version()['version_number'] < 0x081001 &&
        php_uname('s') === 'Darwin',
    'cURL < 8.16.1 is buggy on macOS'
);
?>
--FILE--
<?php
require_once __DIR__ . '/../include/bootstrap.php';

use Swow\Channel;
use Swow\Coroutine;

$certificate = __DIR__ . '/cancel_server.pem';
$ca = __DIR__ . '/cancel_ca.pem';
$generator = new CertificateGenerator(false);
$generator->saveCaCert($ca);
$generator->saveNewCertAsFileWithKey('localhost', $certificate, 'prime256v1', 'IP:127.0.0.1,DNS:localhost', 'critical,serverAuth');
$requestReceived = new Channel(1);
$releaseResponse = new Channel(1);
[$server, $serverCoroutine] = createHttpTestServer($certificate, onRequest: static function (string $request) use ($requestReceived, $releaseResponse): void {
    $requestReceived->push(true);
    $releaseResponse->pop();
});
$url = 'https://127.0.0.1:' . $server->getSockPort() . '/transfer';

try {
    foreach ([false, true] as $schedule) {
        $ch = curl_init();
        curl_setopt($ch, CURLOPT_URL, $url);
        curl_setopt_array($ch, [CURLOPT_PROXY => '', CURLOPT_TIMEOUT_MS => 5000, CURLOPT_CAINFO => $ca, CURLOPT_SSL_VERIFYPEER => true, CURLOPT_SSL_VERIFYHOST => 2]);
        curl_setopt($ch, CURLOPT_HEADER, 0);
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
        $response = null;
        $coroutine = Coroutine::run(static function () use ($ch, &$response): void {
            $response = curl_exec($ch);
        });
        // Cancel a real transfer waiting for its response; loopback must not complete it first.
        $requestReceived->pop();
        // cancel
        if ($schedule) {
            sleep(0);
        }
        $coroutine->resume();
        Assert::false($response);
        $releaseResponse->push(true);
    }
} finally {
    unset($ch, $coroutine);
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
foreach (['cancel_server.pem', 'cancel_server.pem.cnf', 'cancel_ca.pem'] as $file) {
    @unlink(__DIR__ . '/' . $file);
}
?>
--EXPECT--
Done
