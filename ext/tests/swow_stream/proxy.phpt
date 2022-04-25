--TEST--
swow_stream: proxy
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_offline();
require __DIR__ . '/../include/bootstrap.php';
skip_if(
    ($response = httpRequest('https://github.com', timeoutSeconds: 3, proxy: true, doNotThrow: true)) === false || $response['status'] !== 200,
    'Proxy is not available'
);
?>
--FILE--
<?php
require_once __DIR__ . '/../include/bootstrap.php';

use Swow\Http\Status;

$response = httpRequest('https://github.com', timeoutSeconds: 10, proxy: true);
Assert::same($response['status'], Status::OK);
Assert::contains($response['body'], 'github');

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
