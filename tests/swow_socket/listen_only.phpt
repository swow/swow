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

$socket = (new Socket)->bind('127.0.0.1')->listen();

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
