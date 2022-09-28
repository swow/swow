--TEST--
swow_socket: listen only
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Socket;

$socket = (new Socket(Socket::TYPE_TCP))->bind('127.0.0.1')->listen();

echo "Done\n";

?>
--EXPECT--
Done
