--TEST--
swow_misc: trig deprecation and errors in coroutine
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip('no proper deprecation in this version of PHP', PHP_VERSION_ID >= 80200 || PHP_VERSION_ID < 70200);
?>
--INI--
memory_limit=128M
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';
use Swow\Coroutine;

Coroutine::run(function () {
    // E_ERROR
    $_ = str_repeat('the quick brown twosee jumps over the lazy black dixyes', 128 * 1048576);
});

echo "Never here\n";
?>
--EXPECTF--
%AFatal error: [Fatal error in R%d] Allowed memory size of %d bytes exhausted%A (tried to allocate %d bytes)
Stack trace:
#%d %swarning_in_coro_3.php(%d): %s
#%d [internal function]: {closure}()%A
#%d %swarning_in_coro_3.php(%d): Swow\Coroutine::run(Object(Closure))
#%d {main}
  triggered in %swarning_in_coro_3.php on line %d
