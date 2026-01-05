--TEST--
swow_siritz: exit2
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';

skip_if_not_zts();
?>
--INI--
swow.thread_exit_join_ms=10
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Siritz;

$thread = new Siritz(function () {
    msleep(10000);
    echo "Child thread exit\n";
});

$thread->run();

echo "Main thread exit\n";

?>
--EXPECT--
Main thread exit
