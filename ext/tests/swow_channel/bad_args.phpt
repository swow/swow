--TEST--
swow_channel: bad arguments passed in
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Channel;

$chan = new Channel(0);
Assert::throws(static function () use ($chan): void {
    // Swow\Channel can be constructed only once
    $chan->__construct();
}, Error::class);

Assert::throws(static function (): void {
    // Swow\Channel::__construct(): Argument #1 ($capacity) can not be negative
    new Channel(-999);
}, ValueError::class);

echo "Done\n";
?>
--EXPECT--
Done
