--TEST--
swow_misc: trig deprecation and errors in coroutine
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip('no proper deprecation in this version of PHP', PHP_VERSION_ID >= 80200 || PHP_VERSION_ID < 70200);
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Sync\WaitReference;

$wr = new WaitReference();

Coroutine::run(function () use ($wr) {
    // E_DEPRECATED
    if (PHP_VERSION_ID < 80200) {
        hexdec('bad_arg-1');
    }
    // E_NOTICE
    $s = serialize(['test' => 1048975]);
    unserialize(substr($s, 0, 12));
    // E_WARNING
    gethostbyname(str_repeat('cafebabe', 1048576));
    // E_COMPILE_WARNING
    eval(/* @lang text */ 'declare(not_a_declare=1);');
    // E_COMPILE_ERROR
    eval(/* @lang text */ 'try{}finally{goto out;}out:');
});

WaitReference::wait($wr);

echo "Never here\n";
?>
--EXPECTF--
Deprecated: [Deprecated in R%d] %s
Stack trace:
#%d %swarning_in_coro_2.php(%d): %s
#%d [internal function]: {closure}()%A
#%d %swarning_in_coro_2.php(%d): Swow\Coroutine::run(Object(Closure))
#%d {main}
  triggered in %swarning_in_coro_2.php on line %d

%ANotice: [Notice in R%d] %s
Stack trace:
#%d %swarning_in_coro_2.php(%d): %s
#%d [internal function]: {closure}()%A
#%d %swarning_in_coro_2.php(%d): Swow\Coroutine::run(Object(Closure))
#%d {main}
  triggered in %swarning_in_coro_2.php on line %d

%AWarning: [Warning in R%d] %s
Stack trace:
#%d %swarning_in_coro_2.php(%d): %s
#%d [internal function]: {closure}()%A
#%d %swarning_in_coro_2.php(%d): Swow\Coroutine::run(Object(Closure))
#%d {main}
  triggered in %swarning_in_coro_2.php on line %d

%AWarning: [Warning in R%d] Unsupported declare 'not_a_declare'
Stack trace:
#%d [internal function]: {closure}()%A
#%d %swarning_in_coro_2.php(%d): Swow\Coroutine::run(Object(Closure))
#%d {main}
  triggered in %swarning_in_coro_2.php(%d) : eval()'d code on line 1

%AFatal error: [Fatal error in R%d] jump out of a finally block is disallowed
Stack trace:
#%d [internal function]: {closure}()%A
#%d %swarning_in_coro_2.php(%d): Swow\Coroutine::run(Object(Closure))
#%d {main}
  triggered in %swarning_in_coro_2.php(%d) : eval()'d code on line 1
