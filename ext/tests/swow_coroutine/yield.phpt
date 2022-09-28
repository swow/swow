--TEST--
swow_coroutine: yield
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$coroutine = new Swow\Coroutine(function () {
    echo "In\n";
    Swow\Coroutine::yield();
    echo "End\n";
});
echo "Resume\n";
$coroutine->resume();
echo "Out\n";
$coroutine->resume();
echo "Done\n";

?>
--EXPECT--
Resume
In
Out
End
Done
