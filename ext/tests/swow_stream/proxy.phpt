--TEST--
swow_stream: proxy
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_offline();
require __DIR__ . '/../include/bootstrap.php';
skip('Proxy is not available', httpRequest('https://github.com', timeoutSeconds: 10, proxy: true)['status'] !== 200);
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
