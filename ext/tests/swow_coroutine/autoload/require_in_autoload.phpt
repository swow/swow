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
    var_dump(new Foo());
    usleep(1000); // coroutine context switch
    require __DIR__ . '/Foo.inc';
});

new Foo();

?>
--EXPECTF--
Fatal error: Uncaught Error: Class "%s\Foo" not found in %srequire_in_autoload.php:%d
Stack trace:
#0 %s/require_in_autoload.php(%d): {closure}('%s...')
#1 {main}
  thrown in %srequire_in_autoload.php on line %d
