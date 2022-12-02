--TEST--
swow_coroutine: resume
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$coroutine = new Swow\Coroutine(static function (): void {
    echo "Hello Swow\n";
});
echo "Resume\n";
$coroutine->resume();
echo "Done\n";

?>
--EXPECT--
Resume
Hello Swow
Done
