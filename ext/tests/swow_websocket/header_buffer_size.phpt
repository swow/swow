--TEST--
swow_websocket: header buffer must stay large enough for the whole header
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\WebSocket\Header;

// The header is stored in the inherited Swow\Buffer storage. Shrinking that
// storage must not let the header helpers write past it.
$header = new Header();
$header->realloc(1);
Assert::throws(static function () use ($header): void {
    $header->setPayloadLength(0x7F);
}, Error::class, expectMessage: '/WebSocket header buffer is unavailable/');

$header = new Header();
$header->close();
Assert::throws(static function () use ($header): void {
    $header->setOpcode(1);
}, Error::class, expectMessage: '/WebSocket header buffer is unavailable/');

$header = new Header();
$header->realloc(1);
Assert::throws(static function () use ($header): void {
    $header->getPayloadLength();
}, Error::class, expectMessage: '/WebSocket header buffer is unavailable/');

// A regular header keeps working.
$header = new Header();
$header->setPayloadInfo(payloadLength: 0, maskingKey: '1234');
Assert::same($header->getMask(), true);
Assert::same($header->getMaskingKey(), '1234');

echo "Done\n";

?>
--EXPECT--
Done
