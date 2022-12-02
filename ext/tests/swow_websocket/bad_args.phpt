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
$frame->setPayloadLength(0);
$frame->setMaskingKey('1234');
$frame->setMaskingKey("\0\0\0\0");
$frame->setPayloadInfo(payloadLength: 0, maskingKey: '1234');
$frame->setPayloadInfo(payloadLength: 0, maskingKey: "\0\0\0\0");
Assert::throws(static function () use ($frame): void {
    $frame->setMaskingKey(maskingKey: '1');
}, ValueError::class, expectMessage: '/Argument #\d \(\$maskingKey\) length should be 0 or 4/');
Assert::throws(static function () use ($frame): void {
    $frame->setMaskingKey(maskingKey: '12345');
}, ValueError::class, expectMessage: '/Argument #\d \(\$maskingKey\) length should be 0 or 4/');
Assert::throws(static function () use ($frame): void {
    $frame->setPayloadInfo(payloadLength: 0, maskingKey: '1');
}, ValueError::class, expectMessage: '/Argument #\d \(\$maskingKey\) length should be 0 or 4/');
Assert::throws(static function () use ($frame): void {
    $frame->setPayloadInfo(payloadLength: 0, maskingKey: '12345');
}, ValueError::class, expectMessage: '/Argument #\d \(\$maskingKey\) length should be 0 or 4/');

echo "Done\n";

?>
--EXPECT--
Done
