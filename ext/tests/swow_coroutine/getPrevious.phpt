--TEST--
swow_coroutine: getPrevious
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$main = Swow\Coroutine::getCurrent();
$coroutine = new Swow\Coroutine(function () use ($main, &$coroutine) {
    if ($main === $coroutine->getPrevious()) {
        echo "Success\n";
    }
    echo "Yield\n";
    $main->resume();
    echo "End\n";
});
echo "Resume-1\n";
$coroutine->resume();
echo "Resume-2\n";
$coroutine->resume();
echo "Done\n";

?>
--EXPECT--
Resume-1
Success
Yield
Resume-2
End
Done
