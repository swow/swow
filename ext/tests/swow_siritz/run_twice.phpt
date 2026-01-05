--TEST--
swow_siritz: run twice
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
    echo "Child hello\n";
});

$thread->run();

Assert::throws(function () use ($thread) {
    $thread->run();
}, SiritzException::class, 0, 'Thread already started');


echo "Done\n";

?>
--EXPECT--
Child hello
Done
