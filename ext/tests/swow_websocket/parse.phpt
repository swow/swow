--TEST--
swow_websocket: basic parser usage
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Buffer;
use Swow\WebSocket\Frame;

const FIN = 1 << 7;
const RSV1 = 1 << 6;
const RSV2 = 1 << 5;
const RSV3 = 1 << 4;

const OPCODE_TEXT = 1;
const OPCODE_BIN = 2;
const OPCODE_CLOSE = 8;
const OPCODE_PING = 9;
const OPCODE_PONG = 0xa;

const MASK_BIT = 1 << 7;

$buffer = new Buffer(1048576);

// fin, close, len = 0
$buffer->rewind();
$buffer->write(pack('C2', FIN | OPCODE_CLOSE, 0));
$buffer->rewind();

$frame = new Frame();
$headerLength = $frame->unpackHeader($buffer);

// debugInfo
var_dump($frame);
Assert::same($headerLength, 2);
Assert::true($frame->getFin());
Assert::false($frame->getRSV1());
Assert::false($frame->getRSV2());
Assert::false($frame->getRSV3());
Assert::same($frame->getOpcode(), 8);
Assert::false($frame->getMask());
Assert::same($frame->getMaskKey(), '');
Assert::same($frame->getPayloadLength(), 0);
Assert::same($frame->getHeaderLength(), 2);
Assert::same($buffer->tell(), $headerLength);
Assert::same($frame->getPayloadDataAsString(), '');

// fin, ping, len = 0
$buffer->rewind();
$buffer->write(pack('C2', FIN | OPCODE_PING, 0));
$buffer->rewind();

$frame = new Frame();
$headerLength = $frame->unpackHeader($buffer);

// debugInfo
var_dump($frame);
Assert::same($headerLength, 2);
Assert::true($frame->getFin());
Assert::false($frame->getRSV1());
Assert::false($frame->getRSV2());
Assert::false($frame->getRSV3());
Assert::same($frame->getOpcode(), 9);
Assert::false($frame->getMask());
Assert::same($frame->getMaskKey(), '');
Assert::same($frame->getPayloadLength(), 0);
Assert::same($frame->getHeaderLength(), 2);
Assert::same($buffer->tell(), $headerLength);
Assert::same($frame->getPayloadDataAsString(), '');

// fin, pong, len = 0
$buffer->rewind();
$buffer->write(pack('C2', FIN | OPCODE_PONG, 0));
$buffer->rewind();

$frame = new Frame();
$headerLength = $frame->unpackHeader($buffer);

// debugInfo
var_dump($frame);
Assert::same($headerLength, 2);
Assert::true($frame->getFin());
Assert::false($frame->getRSV1());
Assert::false($frame->getRSV2());
Assert::false($frame->getRSV3());
Assert::same($frame->getOpcode(), 0xa);
Assert::false($frame->getMask());
Assert::same($frame->getMaskKey(), '');
Assert::same($frame->getPayloadLength(), 0);
Assert::same($frame->getHeaderLength(), 2);
Assert::same($buffer->tell(), $headerLength);
Assert::same($frame->getPayloadDataAsString(), '');

// fin, text message, len=16, payload = 'the data sent 16'
$buffer->rewind();
$buffer->write(pack('C2', FIN | OPCODE_TEXT, 16));
$buffer->write('the data sent 16');
$buffer->rewind();

$frame = new Frame();
$headerLength = $frame->unpackHeader($buffer);

// debugInfo
var_dump($frame);
Assert::same($headerLength, 2);
Assert::true($frame->getFin());
Assert::false($frame->getRSV1());
Assert::false($frame->getRSV2());
Assert::false($frame->getRSV3());
Assert::same($frame->getOpcode(), 1);
Assert::false($frame->getMask());
Assert::same($frame->getMaskKey(), '');
Assert::same($payloadLength = $frame->getPayloadLength(), 16);
Assert::same($frame->getHeaderLength(), 2);
Assert::same($buffer->tell(), $headerLength);
$payloadBuffer = $frame->getPayloadData();
$payloadBuffer->realloc($payloadLength);
$payloadBuffer->write($buffer->peekFrom($headerLength, $payloadLength));
$frame->unmaskPayloadData();
$payloadBuffer->rewind();
Assert::same($payloadBuffer->read(), 'the data sent 16');
Assert::same($frame->getPayloadDataAsString(), 'the data sent 16');
// buffer filled debugInfo
var_dump($frame);

// fin, rsv1, text message, masked, len=16, payload = 'the data sent 16', masking key = [0x80 0x81 0x82 0x84]
$buffer->rewind();
$buffer->write(pack('C2', FIN | RSV1 | OPCODE_TEXT, MASK_BIT | 16));
$buffer->write("\x80\x81\x82\x84");
$message = '';
$parts = unpack('N4', 'the data sent 16');
foreach ($parts as $part) {
    $message .= pack('N', $part ^ 0x80818284);
}
$buffer->write($message);
$buffer->rewind();

$frame = new Frame();
$headerLength = $frame->unpackHeader($buffer);

var_dump($frame);
Assert::same($headerLength, 2 + 4/* masking key */);
Assert::true($frame->getFin());
Assert::true($frame->getRSV1());
Assert::false($frame->getRSV2());
Assert::false($frame->getRSV3());
Assert::same($frame->getOpcode(), 1);
Assert::true($frame->getMask());
Assert::same($frame->getMaskKey(), "\x80\x81\x82\x84");
Assert::same($payloadLength = $frame->getPayloadLength(), 16);
Assert::same($frame->getHeaderLength(), 6);
Assert::same($buffer->tell(), $headerLength);
$payloadBuffer = $frame->getPayloadData();
$payloadBuffer->realloc($payloadLength);
$payloadBuffer->write($buffer->peekFrom($headerLength, $payloadLength));
$frame->unmaskPayloadData();
$payloadBuffer->rewind();
Assert::same($payloadBuffer->read(), 'the data sent 16');
Assert::same($frame->getPayloadDataAsString(), 'the data sent 16');

    // rsv2, binary message, masked, len=256, payload = '\xca\xfe\xba\xbe\xde\xad\xbe\xef' * 32
$buffer->rewind();
$buffer->write(pack('C2', RSV2 | OPCODE_BIN, MASK_BIT | 0x7e));
$buffer->write(pack('n', 256));
$buffer->write("\x10\x21\x32\x44");
$message = '';
$parts = unpack('N*', str_repeat("\xca\xfe\xba\xbe\xde\xad\xbe\xef", 32));
foreach ($parts as $part) {
    $message .= pack('N', $part ^ 0x10213244);
}
$buffer->write($message);
$buffer->rewind();

$frame = new Frame();
$headerLength = $frame->unpackHeader($buffer);

var_dump($frame);
Assert::same($headerLength, 4 + 4/* masking key */);
Assert::false($frame->getFin());
Assert::false($frame->getRSV1());
Assert::true($frame->getRSV2());
Assert::false($frame->getRSV3());
Assert::same($frame->getOpcode(), 2);
Assert::true($frame->getMask());
Assert::same($frame->getMaskKey(), "\x10\x21\x32\x44");
Assert::same($payloadLength = $frame->getPayloadLength(), 256);
Assert::same($frame->getHeaderLength(), 8);
Assert::same($buffer->tell(), $headerLength);
$payloadBuffer = $frame->getPayloadData();
$payloadBuffer->realloc($payloadLength);
$payloadBuffer->write($buffer->peekFrom($headerLength, $payloadLength));
$frame->unmaskPayloadData();
$payloadBuffer->rewind();
Assert::same($payloadBuffer->read(), str_repeat("\xca\xfe\xba\xbe\xde\xad\xbe\xef", 32));
Assert::same($frame->getPayloadDataAsString(), str_repeat("\xca\xfe\xba\xbe\xde\xad\xbe\xef", 32));

// fin, rsv3, binary message, len=65536, payload = '\xf0\x0f\xc7\xc8\x2f\x90\x2f\x90' * 8192
$buffer->rewind();
$buffer->write(pack('C2', FIN | RSV3 | OPCODE_BIN, MASK_BIT | 0x7f));
$buffer->write(pack('J', 65536));
$buffer->write('swow');
$message = '';
$parts = unpack('N*', str_repeat("\xf0\x0f\xc7\xc8\x2f\x90\x2f\x90", 8192));
foreach ($parts as $part) {
    $message .= pack('N', $part ^ 0x73776f77);
}
$buffer->write($message);
$buffer->rewind();

$frame = new Frame();
$headerLength = $frame->unpackHeader($buffer);

var_dump($frame);
Assert::same($headerLength, 10 + 4/* masking key */);
Assert::true($frame->getFin());
Assert::false($frame->getRSV1());
Assert::false($frame->getRSV2());
Assert::true($frame->getRSV3());
Assert::same($frame->getOpcode(), 2);
Assert::true($frame->getMask());
Assert::same($frame->getMaskKey(), 'swow');
Assert::same($payloadLength = $frame->getPayloadLength(), 65536);
Assert::same($frame->getHeaderLength(), 14);
Assert::same($buffer->tell(), $headerLength);
$payloadBuffer = $frame->getPayloadData();
$payloadBuffer->realloc($payloadLength);
$payloadBuffer->write($buffer->peekFrom($headerLength, $payloadLength));
$frame->unmaskPayloadData();
$payloadBuffer->rewind();
Assert::same($payloadBuffer->read(), str_repeat("\xf0\x0f\xc7\xc8\x2f\x90\x2f\x90", 8192));
Assert::same($frame->getPayloadDataAsString(), str_repeat("\xf0\x0f\xc7\xc8\x2f\x90\x2f\x90", 8192));

echo 'Done' . PHP_LF;

?>
--EXPECTF--
object(Swow\WebSocket\Frame)#%d (7) {
  ["opcode"]=>
  int(8)
  ["fin"]=>
  bool(true)
  ["rsv1"]=>
  bool(false)
  ["rsv2"]=>
  bool(false)
  ["rsv3"]=>
  bool(false)
  ["mask"]=>
  bool(false)
  ["payload_length"]=>
  int(0)
}
object(Swow\WebSocket\Frame)#%d (7) {
  ["opcode"]=>
  int(9)
  ["fin"]=>
  bool(true)
  ["rsv1"]=>
  bool(false)
  ["rsv2"]=>
  bool(false)
  ["rsv3"]=>
  bool(false)
  ["mask"]=>
  bool(false)
  ["payload_length"]=>
  int(0)
}
object(Swow\WebSocket\Frame)#%d (7) {
  ["opcode"]=>
  int(10)
  ["fin"]=>
  bool(true)
  ["rsv1"]=>
  bool(false)
  ["rsv2"]=>
  bool(false)
  ["rsv3"]=>
  bool(false)
  ["mask"]=>
  bool(false)
  ["payload_length"]=>
  int(0)
}
object(Swow\WebSocket\Frame)#%d (7) {
  ["opcode"]=>
  int(1)
  ["fin"]=>
  bool(true)
  ["rsv1"]=>
  bool(false)
  ["rsv2"]=>
  bool(false)
  ["rsv3"]=>
  bool(false)
  ["mask"]=>
  bool(false)
  ["payload_length"]=>
  int(16)
}
object(Swow\WebSocket\Frame)#%d (8) {
  ["opcode"]=>
  int(1)
  ["fin"]=>
  bool(true)
  ["rsv1"]=>
  bool(false)
  ["rsv2"]=>
  bool(false)
  ["rsv3"]=>
  bool(false)
  ["mask"]=>
  bool(false)
  ["payload_length"]=>
  int(16)
  ["payload_data"]=>
  object(Swow\Buffer)#%d (4) {
    ["value"]=>
    string(16) "the data sent 16"
    ["size"]=>
    int(16)
    ["length"]=>
    int(16)
    ["offset"]=>
    int(16)
  }
}
object(Swow\WebSocket\Frame)#%d (8) {
  ["opcode"]=>
  int(1)
  ["fin"]=>
  bool(true)
  ["rsv1"]=>
  bool(true)
  ["rsv2"]=>
  bool(false)
  ["rsv3"]=>
  bool(false)
  ["mask"]=>
  bool(true)
  ["payload_length"]=>
  int(16)
  ["mask_key"]=>
  string(19) "0x80 0x81 0x82 0x84"
}
object(Swow\WebSocket\Frame)#%d (8) {
  ["opcode"]=>
  int(2)
  ["fin"]=>
  bool(false)
  ["rsv1"]=>
  bool(false)
  ["rsv2"]=>
  bool(true)
  ["rsv3"]=>
  bool(false)
  ["mask"]=>
  bool(true)
  ["payload_length"]=>
  int(256)
  ["mask_key"]=>
  string(19) "0x10 0x21 0x32 0x44"
}
object(Swow\WebSocket\Frame)#%d (8) {
  ["opcode"]=>
  int(2)
  ["fin"]=>
  bool(true)
  ["rsv1"]=>
  bool(false)
  ["rsv2"]=>
  bool(false)
  ["rsv3"]=>
  bool(true)
  ["mask"]=>
  bool(true)
  ["payload_length"]=>
  int(65536)
  ["mask_key"]=>
  string(19) "0x73 0x77 0x6F 0x77"
}
Done
