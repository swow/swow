--TEST--
swow_websocket: basic parser usage
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\WebSocket\Header;
use Swow\WebSocket\WebSocket;

const FIN = 1 << 7;
const RSV1 = 1 << 6;
const RSV2 = 1 << 5;
const RSV3 = 1 << 4;

const OPCODE_TEXT = 1;
const OPCODE_BIN = 2;
const OPCODE_CLOSE = 8;
const OPCODE_PING = 9;
const OPCODE_PONG = 0xA;

const MASK_BIT = 1 << 7;

// fin, close, len = 0
$header = new Header();
$header->write(0, pack('C2', FIN | OPCODE_CLOSE, 0));

// debugInfo
var_dump($header);
Assert::same($header->getHeaderSize(), 2);
Assert::true($header->getFin());
Assert::false($header->getRSV1());
Assert::false($header->getRSV2());
Assert::false($header->getRSV3());
Assert::same($header->getOpcode(), 8);
Assert::false($header->getMask());
Assert::same($header->getMaskingKey(), '');
Assert::same($header->getPayloadLength(), 0);
Assert::same($header->getHeaderSize(), 2);
Assert::same($header->getLength(), $header->getHeaderSize());

// fin, ping, len = 0
$header->clear();
$header->append(pack('C2', FIN | OPCODE_PING, 0));

// debugInfo
var_dump($header);
Assert::same($header->getHeaderSize(), 2);
Assert::true($header->getFin());
Assert::false($header->getRSV1());
Assert::false($header->getRSV2());
Assert::false($header->getRSV3());
Assert::same($header->getOpcode(), 9);
Assert::false($header->getMask());
Assert::same($header->getMaskingKey(), '');
Assert::same($header->getPayloadLength(), 0);
Assert::same($header->getHeaderSize(), 2);
Assert::same($header->getLength(), $header->getHeaderSize());

// fin, pong, len = 0
$header->clear();
$header->append(pack('C2', FIN | OPCODE_PONG, 0));
// debugInfo
var_dump($header);
Assert::same($header->getHeaderSize(), 2);
Assert::true($header->getFin());
Assert::false($header->getRSV1());
Assert::false($header->getRSV2());
Assert::false($header->getRSV3());
Assert::same($header->getOpcode(), 0xA);
Assert::false($header->getMask());
Assert::same($header->getMaskingKey(), '');
Assert::same($header->getPayloadLength(), 0);
Assert::same($header->getHeaderSize(), 2);
Assert::same($header->getLength(), $header->getHeaderSize());

// fin, text message, len=16, payload = 'the data sent 16'
$header->clear();
$header->append(pack('C2', FIN | OPCODE_TEXT, 16));
$header->append('the data sent 16');
// debugInfo
var_dump($header);
Assert::same($header->getHeaderSize(), 2);
Assert::true($header->getFin());
Assert::false($header->getRSV1());
Assert::false($header->getRSV2());
Assert::false($header->getRSV3());
Assert::same($header->getOpcode(), 1);
Assert::false($header->getMask());
Assert::same($header->getMaskingKey(), '');
Assert::same($payloadLength = $header->getPayloadLength(), 16);
Assert::same($header->getHeaderSize(), 2);
Assert::same($header->getLength(), $header->getHeaderSize() + 16);
WebSocket::unmask($header, $header->getHeaderSize());
Assert::same($header->read($header->getHeaderSize()), 'the data sent 16');
// buffer filled debugInfo
var_dump($header);

// fin, rsv1, text message, masked, len=16, payload = 'the data sent 16', masking key = [0x80 0x81 0x82 0x84]
$header->clear();
$header->append(pack('C2', FIN | RSV1 | OPCODE_TEXT, MASK_BIT | 16));
$header->append("\x80\x81\x82\x84");
$message = '';
$parts = unpack('N4', 'the data sent 16');
foreach ($parts as $part) {
    $message .= pack('N', $part ^ 0x80818284);
}
$header->append($message);
// debugInfo
var_dump($header);
Assert::same($header->getHeaderSize(), 2 + 4/* masking key */);
Assert::true($header->getFin());
Assert::true($header->getRSV1());
Assert::false($header->getRSV2());
Assert::false($header->getRSV3());
Assert::same($header->getOpcode(), 1);
Assert::true($header->getMask());
Assert::same($header->getMaskingKey(), "\x80\x81\x82\x84");
Assert::same($payloadLength = $header->getPayloadLength(), 16);
Assert::same($header->getHeaderSize(), 6);
Assert::same($header->getLength(), $header->getHeaderSize() + 16);
WebSocket::unmask($header, $header->getHeaderSize(), maskingKey: $header->getMaskingKey());
Assert::same($header->read($header->getHeaderSize()), 'the data sent 16');

// rsv2, binary message, masked, len=256, payload = '\xca\xfe\xba\xbe\xde\xad\xbe\xef' * 32
$header->clear();
$header->append(pack('C2', RSV2 | OPCODE_BIN, MASK_BIT | 0x7E));
$header->append(pack('n', 256));
$header->append("\x10\x21\x32\x44");
$message = '';
$parts = unpack('N*', str_repeat("\xca\xfe\xba\xbe\xde\xad\xbe\xef", 32));
foreach ($parts as $part) {
    $message .= pack('N', $part ^ 0x10213244);
}
$header->append($message);
// debugInfo
var_dump($header);
Assert::same($header->getHeaderSize(), 4 + 4/* masking key */);
Assert::false($header->getFin());
Assert::false($header->getRSV1());
Assert::true($header->getRSV2());
Assert::false($header->getRSV3());
Assert::same($header->getOpcode(), 2);
Assert::true($header->getMask());
Assert::same($header->getMaskingKey(), "\x10\x21\x32\x44");
Assert::same($payloadLength = $header->getPayloadLength(), 256);
Assert::same($header->getHeaderSize(), 8);
Assert::same($header->getLength(), $header->getHeaderSize() + 256);
WebSocket::unmask($header, $header->getHeaderSize(), maskingKey: $header->getMaskingKey());
Assert::same($header->read($header->getHeaderSize()), str_repeat("\xca\xfe\xba\xbe\xde\xad\xbe\xef", 32));

// fin, rsv3, binary message, len=65536, payload = '\xf0\x0f\xc7\xc8\x2f\x90\x2f\x90' * 8192
$header->clear();
$header->append(pack('C2', FIN | RSV3 | OPCODE_BIN, MASK_BIT | 0x7F));
$header->append(pack('J', 65536));
$header->append('swow');
$message = '';
$parts = unpack('N*', str_repeat("\xf0\x0f\xc7\xc8\x2f\x90\x2f\x90", 8192));
foreach ($parts as $part) {
    $message .= pack('N', $part ^ 0x73776F77);
}
$header->append($message);
// debugInfo
var_dump($header);
Assert::same($header->getHeaderSize(), 10 + 4/* masking key */);
Assert::true($header->getFin());
Assert::false($header->getRSV1());
Assert::false($header->getRSV2());
Assert::true($header->getRSV3());
Assert::same($header->getOpcode(), 2);
Assert::true($header->getMask());
Assert::same($header->getMaskingKey(), 'swow');
Assert::same($payloadLength = $header->getPayloadLength(), 65536);
Assert::same($header->getHeaderSize(), 14);
Assert::same($header->getLength(), $header->getHeaderSize() + 65536);
WebSocket::unmask($header, $header->getHeaderSize(), maskingKey: $header->getMaskingKey());
Assert::same($header->read($header->getHeaderSize()), str_repeat("\xf0\x0f\xc7\xc8\x2f\x90\x2f\x90", 8192));
echo "Done\n";

?>
--EXPECTF--
object(Swow\WebSocket\Header)#%d (7) {
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
object(Swow\WebSocket\Header)#%d (7) {
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
object(Swow\WebSocket\Header)#%d (7) {
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
object(Swow\WebSocket\Header)#%d (7) {
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
object(Swow\WebSocket\Header)#%d (7) {
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
object(Swow\WebSocket\Header)#%d (8) {
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
  ["masking_key"]=>
  string(16) "\x80\x81\x82\x84"
}
object(Swow\WebSocket\Header)#%d (8) {
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
  ["masking_key"]=>
  string(16) "\x10\x21\x32\x44"
}
object(Swow\WebSocket\Header)#%d (8) {
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
  ["masking_key"]=>
  string(4) "swow"
}
Done
