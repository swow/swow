--TEST--
swow_socket: invalid type
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Socket;
use Swow\SocketException;

foreach ([-1, 12345] as $type) {
    Assert::throws(static function () use ($type): void {
        new Socket($type);
    }, SocketException::class, expectMessage: '/Invalid socket type/');
}

echo "Done\n";

?>
--EXPECT--
Done
