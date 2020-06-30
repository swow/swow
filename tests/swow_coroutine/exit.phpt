--TEST--
swow_coroutine: exit
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

Swow\Coroutine::run(function () {
    echo '1' . PHP_LF;
    exit;
    /** @noinspection PhpUnreachableStatementInspection */
    echo 'Never here' . PHP_LF;
});

Swow\Coroutine::run(function () {
    echo '2' . PHP_LF;
    exit(0);
    /** @noinspection PhpUnreachableStatementInspection */
    echo 'Never here' . PHP_LF;
});

Swow\Coroutine::run(function () {
    echo '3' . PHP_LF;
    exit(null);
    /** @noinspection PhpUnreachableStatementInspection */
    echo 'Never here' . PHP_LF;
});

Swow\Coroutine::run(function () {
    echo '4' . PHP_LF;
    exit(new stdClass());
    /** @noinspection PhpUnreachableStatementInspection */
    echo 'Never here' . PHP_LF;
});

Swow\Coroutine::run(function () {
    echo '5' . PHP_LF;
    exit(1);
    /** @noinspection PhpUnreachableStatementInspection */
    echo 'Never here' . PHP_LF;
});

?>
--EXPECTF--
1
2
3
4
5

Warning: [Fatal error in R%d] Uncaught Swow\Coroutine\TermException: Exited with code 1 in %s:%d
Stack trace:
#0 [internal function]: {closure}()
#1 {main}
  thrown in %s on line %d
