--TEST--
swow_coroutine: exit
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$coroutine = Swow\Coroutine::run(static function (): void {
    echo "0\n";
    exit;
    /* @noinspection PhpUnreachableStatementInspection */
    echo "Never here\n";
});
Assert::same($coroutine->getExitStatus(), 0);

$coroutine = Swow\Coroutine::run(static function (): void {
    echo "1\n";
    exit(0);
    /* @noinspection PhpUnreachableStatementInspection */
    echo "Never here\n";
});
Assert::same($coroutine->getExitStatus(), 0);

$coroutine = Swow\Coroutine::run(static function (): void {
    echo "2\n";
    exit(null);
    /* @noinspection PhpUnreachableStatementInspection */
    echo "Never here\n";
});
Assert::same($coroutine->getExitStatus(), 0);

$coroutine = Swow\Coroutine::run(static function (): void {
    echo "3\n";
    exit(1);
    /* @noinspection PhpUnreachableStatementInspection */
    echo "Never here\n";
});
Assert::same($coroutine->getExitStatus(), 1);

echo "Done\n";

?>
--EXPECTF--
0
1
2
3
Done
