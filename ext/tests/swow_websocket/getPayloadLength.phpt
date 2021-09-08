--TEST--
swow_websocket: getPayloadLength
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Buffer;
use Swow\WebSocket\Frame;

$frame = new Frame();

// if no buffer set, length is 0
Assert::same($frame->getPayloadLength(), 0);
$buffer = new Buffer(0);
$frame->setPayloadData($buffer);
// if buffer size == 0, length is 0
Assert::same($frame->getPayloadLength(), 0);
$frame->setPayloadData($buffer);
$frame->setPayloadData(null);
// if no buffer set, length is 0
Assert::same($frame->getPayloadLength(), 0);
Assert::same($frame->toString(), "\x81\x00");
$frame->setPayloadLength(1234);
// if length is set
Assert::same($frame->getPayloadLength(), 1234);
$buffer->realloc(8192);
$buffer->write(str_repeat('ceshi', 12));
$frame->setPayloadData($buffer);
// if buffer is set, payload length is buffer length
Assert::same($frame->getPayloadLength(), 60);
$frame->setPayloadLength(2);
// if buffer is set, setPayloadLength will be ignored
Assert::same($frame->getPayloadLength(), 60);
Assert::same($frame->toString(), "\x81\x3c" . str_repeat('ceshi', 12));

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
