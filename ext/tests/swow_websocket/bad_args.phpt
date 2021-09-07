--TEST--
swow_websocket: bad args passed in
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\WebSocket\Frame;

$frame = new Frame();
$frame->setMaskKey('');
Assert::throws(function () use ($frame) {
    $frame->setMaskKey('1');
}, ValueError::class);
Assert::throws(function () use ($frame) {
    $frame->setMaskKey('12345');
}, ValueError::class);

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
