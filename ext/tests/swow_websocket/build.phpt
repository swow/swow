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
use Swow\WebSocket\Header;
use Swow\WebSocket\Opcode;

$buffer = new Buffer(1048576);

// fin, close, len = 0
$header = new Header();
$header
    ->setFin(true)
    ->setOpcode(Opcode::CLOSE);
Assert::same($header->getHeaderSize(), 2);
Assert::same($header->toString(), "\x88\0");

// fin, ping, len = 0
$header = new Header();
$header
    ->setFin(true)
    ->setOpcode(Opcode::PING);
Assert::same($header->getHeaderSize(), 2);
Assert::same($header->toString(), "\x89\0");

// fin, pong, len = 0
$header = new Header();
$header
    ->setFin(true)
    ->setOpcode(Opcode::PONG);
Assert::same($header->getHeaderSize(), 2);
Assert::same($header->toString(), "\x8a\0");

// fin, text message, len=16, payload = 'the data sent 16'
$buffer->append('the data sent 16');
$header = new Header();
$header
    ->setFin(true)
    ->setOpcode(Opcode::TEXT)
    ->setPayloadInfo($buffer->getLength());
Assert::same($header->getHeaderSize(), 2);
Assert::same($header->toString(), "\x81\x10");
$buffer->clear();

// fin, rsv1, text message, masked, len=16, payload = 'the data sent 16', masking key = [0x80 0x81 0x82 0x84]
$message = 'the data sent 16';
$maskedMessage = '';
foreach (unpack('N*', $message) as $part) {
    $maskedMessage .= pack('N', 0x80818284 ^ $part);
}
$buffer->append($message);
$header = new Header();
$header
    ->setFin(true)
    ->setRSV1(true)
    ->setOpcode(Opcode::TEXT)
    ->setPayloadInfo(payloadLength: $buffer->getLength(), maskKey: "\x80\x81\x82\x84");
Assert::same($header->getHeaderSize(), 6);
Assert::same($header->toString(), "\xC1\x90\x80\x81\x82\x84");
$buffer->clear();

// rsv2, binary message, masked, len=256, payload = '\xca\xfe\xba\xbe\xde\xad\xbe\xef' * 32
$message = str_repeat("\xca\xfe\xba\xbe\xde\xad\xbe\xef", 32);
$maskedMessage = '';
foreach (unpack('N*', $message) as $part) {
    $maskedMessage .= pack('N', 0x10213244 ^ $part);
}
$buffer->append($message);
$header = new Header();
$header
    ->setRSV2(true)
    ->setOpcode(Opcode::BINARY)
    ->setPayloadInfo(payloadLength: $buffer->getLength(), maskKey: "\x10\x21\x32\x44");
Assert::same($header->getHeaderSize(), 8);
Assert::same($header->toString(), "\xA2\xfe\x01\x00\x10\x21\x32\x44");
$buffer->clear();

// fin, rsv3, binary message, len=65536, payload = '\xf0\x0f\xc7\xc8\x2f\x90\x2f\x90' * 8192
$message = str_repeat("\xf0\x0f\xc7\xc8\x2f\x90\x2f\x90", 8192);
$maskedMessage = '';
foreach (unpack('N*', $message) as $part) {
    $maskedMessage .= pack('N', 0x73776F77 ^ $part);
}
$buffer->append($message);
$header = new Header();
$header
    ->setFin(true)
    ->setRSV3(true)
    ->setOpcode(Opcode::BINARY)
    ->setPayloadInfo(payloadLength: $buffer->getLength(), maskKey: strtolower(Swow::class));
Assert::same($header->getHeaderSize(), 14);
Assert::same($header->toString(), "\x92\xff\x00\x00\x00\x00\x00\x01\x00\x00swow");
$header = new Header(
    opcode: Opcode::BINARY,
    payloadLength: $buffer->getLength(),
    maskKey: strtolower(Swow::class),
    fin: true, rsv3: true
);
Assert::same($header->getHeaderSize(), 14);
Assert::same($header->toString(), "\x92\xff\x00\x00\x00\x00\x00\x01\x00\x00swow");

echo "Done\n";
?>
--EXPECT--
Done
