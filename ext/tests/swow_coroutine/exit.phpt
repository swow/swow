--TEST--
swow_coroutine: exit
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$coroutine = Swow\Coroutine::run(function () {
    echo '0' . PHP_LF;
    exit;
    /** @noinspection PhpUnreachableStatementInspection */
    echo 'Never here' . PHP_LF;
});
Assert::same($coroutine->getExitStatus(), 0);

$coroutine = Swow\Coroutine::run(function () {
    echo '1' . PHP_LF;
    exit(0);
    /** @noinspection PhpUnreachableStatementInspection */
    echo 'Never here' . PHP_LF;
});
Assert::same($coroutine->getExitStatus(), 0);

$coroutine = Swow\Coroutine::run(function () {
    echo '2' . PHP_LF;
    exit(null);
    /** @noinspection PhpUnreachableStatementInspection */
    echo 'Never here' . PHP_LF;
});
Assert::same($coroutine->getExitStatus(), 0);

$coroutine = Swow\Coroutine::run(function () {
    echo '3' . PHP_LF;
    exit(1);
    /** @noinspection PhpUnreachableStatementInspection */
    echo 'Never here' . PHP_LF;
});
Assert::same($coroutine->getExitStatus(), 1);

echo 'Done' . PHP_LF;

?>
--EXPECTF--
0
1
2
3
Done
