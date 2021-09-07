--TEST--
swow_websocket: basic builder usage
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Buffer;
use Swow\WebSocket\Frame;
use Swow\WebSocket\Opcode;

$buffer = new Buffer(1048576);

// fin, close, len = 0
$frame = new Frame();
$frame
    ->setFin(1)
    ->setOpcode(Opcode::CLOSE)
    ->setPayloadLength($buffer->getLength())
    ->setPayloadData($buffer);
Assert::same($frame->getHeaderLength(), 2);
Assert::same($frame->toString(true), "\x88\0");
Assert::same($frame->toString(false), "\x88\0");

// fin, ping, len = 0
$frame = new Frame();
$frame
    ->setFin(1)
    ->setOpcode(Opcode::PING)
    ->setPayloadLength($buffer->getLength())
    ->setPayloadData($buffer);
Assert::same($frame->getHeaderLength(), 2);
Assert::same($frame->toString(true), "\x89\0");
Assert::same($frame->toString(false), "\x89\0");

// fin, pong, len = 0
$frame = new Frame();
$frame
    ->setFin(1)
    ->setOpcode(Opcode::PONG)
    ->setPayloadLength($buffer->getLength())
    ->setPayloadData($buffer);
Assert::same($frame->getHeaderLength(), 2);
Assert::same($frame->toString(true), "\x8a\0");
Assert::same($frame->toString(false), "\x8a\0");

// fin, text message, len=16, payload = 'the data sent 16'
$buffer->write('the data sent 16');
$buffer->rewind();
$frame = new Frame();
$frame
    ->setFin(1)
    ->setOpcode(Opcode::TEXT)
    ->setPayloadLength($buffer->getLength())
    ->setPayloadData($buffer);
Assert::same($frame->getHeaderLength(), 2);
Assert::same($frame->toString(true), "\x81\x10");
Assert::same($frame->toString(false), "\x81\x10the data sent 16");

// fin, rsv1, text message, masked, len=16, payload = 'the data sent 16', masking key = [0x80 0x81 0x82 0x84]
$message = 'the data sent 16';
$maskedMessage = '';
foreach (unpack('N*', $message) as $part) {
    $maskedMessage .= pack('N', 0x80818284 ^ $part);
}
$buffer->write($message);
$buffer->rewind();
$frame = new Frame();
$frame
    ->setFin(1)
    ->setRSV1(1)
    ->setOpcode(Opcode::TEXT)
    ->setMask(true)
    ->setMaskKey("\x80\x81\x82\x84")
    ->setPayloadLength($buffer->getLength())
    ->setPayloadData($buffer);
Assert::same($frame->getHeaderLength(), 6);
Assert::same($frame->toString(true), "\xC1\x90\x80\x81\x82\x84");
Assert::same($frame->toString(false), "\xC1\x90\x80\x81\x82\x84{$maskedMessage}");
$frame->resetHeader();
Assert::same($frame->toString(true), "\x81\x10");

// rsv2, binary message, masked, len=256, payload = '\xca\xfe\xba\xbe\xde\xad\xbe\xef' * 32
$message = str_repeat("\xca\xfe\xba\xbe\xde\xad\xbe\xef", 32);
$maskedMessage = '';
foreach (unpack('N*', $message) as $part) {
    $maskedMessage .= pack('N', 0x10213244 ^ $part);
}
$buffer->write($message);
$buffer->rewind();
$frame = new Frame();
$frame
    ->setRSV2(1)
    ->setOpcode(Opcode::BINARY)
    ->setMask(true)
    ->setMaskKey("\x10\x21\x32\x44")
    ->setPayloadLength($buffer->getLength())
    ->setPayloadData($buffer);
Assert::same($frame->getHeaderLength(), 8);
Assert::same($frame->toString(true), "\xA2\xfe\x01\x00\x10\x21\x32\x44");
Assert::same($frame->toString(false), "\xA2\xfe\x01\x00\x10\x21\x32\x44{$maskedMessage}");
$frame->resetHeader();
Assert::same($frame->toString(true), "\x81\x7e\x01\x00");

// fin, rsv3, binary message, len=65536, payload = '\xf0\x0f\xc7\xc8\x2f\x90\x2f\x90' * 8192
$message = str_repeat("\xf0\x0f\xc7\xc8\x2f\x90\x2f\x90", 8192);
$maskedMessage = '';
foreach (unpack('N*', $message) as $part) {
    $maskedMessage .= pack('N', 0x73776f77 ^ $part);
}
$buffer->write($message);
$buffer->rewind();
$frame = new Frame();
$frame
    ->setFin(1)
    ->setRSV3(1)
    ->setOpcode(Opcode::BINARY)
    ->setMask(true)
    ->setMaskKey('swow')
    ->setPayloadLength($buffer->getLength())
    ->setPayloadData($buffer);
Assert::same($frame->getHeaderLength(), 14);
Assert::same($frame->toString(true), "\x92\xff\x00\x00\x00\x00\x00\x01\x00\x00swow");
Assert::same($frame->toString(false), "\x92\xff\x00\x00\x00\x00\x00\x01\x00\x00swow{$maskedMessage}");
$frame->resetHeader();
Assert::same($frame->toString(true), "\x81\x7F\x00\x00\x00\x00\x00\x01\x00\x00");

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
