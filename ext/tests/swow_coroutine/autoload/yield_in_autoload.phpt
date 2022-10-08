--TEST--
swow_coroutine:
--SKIPIF--
<?php
require __DIR__ . '/../../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Sync\WaitReference;
use SwowTestYieldInAutoload\Foo;

$wr = new WaitReference();

spl_autoload_register(function (string $class): void {
    usleep(1000); // coroutine context switch
    require __DIR__ . '/Foo.inc';
});

for ($i = 0; $i < 3; ++$i) {
    Coroutine::run(static function () use ($wr): void {
        var_dump(new Foo());
    });
}

$wr::wait($wr);

?>
--EXPECTF--
object(%s\Foo)#%d (0) {
}
object(%s\Foo)#%d (0) {
}
object(%s\Foo)#%d (0) {
}
