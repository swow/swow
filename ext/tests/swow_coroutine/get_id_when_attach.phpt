--TEST--
swow_coroutine: getId() when attach
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--XFAIL--
Need to fix
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

$coroutine = Coroutine::run(static function (): void {
    usleep(0);
});
Assert::same(Coroutine::getCurrent()->getId(), Coroutine::getMain()->getId());
$coroutine->call(static function (): void {
    Assert::same(Coroutine::getCurrent()->getId(), Coroutine::getMain()->getId() + 1);
});
$coroutine->eval(<<<'PHP'
    Assert::same(Swow\Coroutine::getCurrent()->getId(), Swow\Coroutine::getMain()->getId() + 1);
PHP
);

echo "Done\n";
?>
--EXPECT--
Done
