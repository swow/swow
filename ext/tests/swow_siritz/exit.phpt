--TEST--
swow_siritz: exit1
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';

skip_if_not_zts();
?>
--INI--
swow.closure_serializer=1
swow.thread_exit_join_ms=-1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Siritz;

$thread = new Siritz(function () {
    for ($i = 0; $i < 100; $i++) {
        // main thread will interrupt this thread using unwind exit
        // so we need to split sleep into small chunks
        msleep(50);
    }
    echo "Child thread exit\n";
});

$thread->run();

echo "Main thread exit\n";

?>
--EXPECT--
Main thread exit
