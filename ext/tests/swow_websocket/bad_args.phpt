--TEST--
swow_websocket: bad args passed in
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\WebSocket\Header;

$frame = new Header();
$frame->setPayloadInfo(payloadLength: 0, maskKey: '1234');
$frame->setPayloadInfo(payloadLength: 0, maskKey: "\0\0\0\0");
Assert::throws(function () use ($frame) {
    $frame->setPayloadInfo(payloadLength: 0, maskKey: '1');
}, ValueError::class, expectMessage: '/Argument #\d \(\$maskKey\) length should be 0 or 4/');
Assert::throws(function () use ($frame) {
    $frame->setPayloadInfo(payloadLength: 0, maskKey: '12345');
}, ValueError::class, expectMessage: '/Argument #\d \(\$maskKey\) length should be 0 or 4/');

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
