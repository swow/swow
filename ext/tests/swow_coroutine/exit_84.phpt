--TEST--
swow_coroutine: exit
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
needs_php_version('>=', '8.4');
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
%Aeprecated: [Deprecated in R%d] exit(): Passing null to parameter #1 ($status) of type string|int is deprecated
Stack trace:
#0 %sexit_84.php(%d): exit(NULL)
#1 [internal function]: {closure:%sexit_84.php:%d}()
#2 %sexit_84.php(%d): Swow\Coroutine::run(Object(Closure))
#3 {main}
  triggered in %sexit_84.php on line %d
3
Done
