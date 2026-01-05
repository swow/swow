--TEST--
swow_siritz: wait twice
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';

skip_if_not_zts();
?>
--INI--
swow.thread_exit_join_ms=-1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Siritz;
use Swow\SiritzException;

$thread = new Siritz(function () {
    msleep(20);
    echo "Child thread exit\n";
});

$thread->run();

$thread->wait();

Assert::throws(function () use ($thread) {
    $thread->wait();
}, SiritzException::class, 0, 'Failed to wait for thread');

echo "Done\n";

?>
--EXPECT--
Child thread exit
Done
