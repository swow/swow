--TEST--
swow_coroutine: kill zombie
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Channel;
use Swow\Coroutine;

$coroutine = Coroutine::run(function () {
    try {
        (new Channel())->pop();
    } catch (Exception $exception) {
        echo "Never here\n";
    }
});

$coroutine->kill();

echo "Done\n";

?>
--EXPECT--
Done
