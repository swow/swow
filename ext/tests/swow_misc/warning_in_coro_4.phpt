--TEST--
swow_misc: trig user warning in coroutine 8.4+
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
needs_php_version('>=', '8.4');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';
use Swow\Coroutine;

Coroutine::run(function (): void {
    // E_PARSE
    eval(/* @lang text */ "i'm a bad expression");
});

Coroutine::run(static function (): void {
    trigger_error('running run-tests.php to test php is deprecated', E_USER_DEPRECATED);
    trigger_error('running run-tests.php to test swow is deprecated', E_USER_NOTICE);
    trigger_error('running run-tests.php to test twosee is deprecated', E_USER_WARNING);
    trigger_error('running run-tests.php to test dixyes is deprecated', E_USER_ERROR);
});

echo "Never here\n";
?>
--EXPECTF--
%A: [Parse error in R%d] %s
Stack trace:%A
#%d %s(%d): Swow\Coroutine::run(Object(Closure))
#%d {main}
  triggered in %s(%d) : eval()'d code on line %d

%ADeprecated: [Deprecated in R%d] running run-tests.php to test php is deprecated
Stack trace:
#%d %swarning_in_coro_4.php(%d): trigger_error('running run-tes...', %d)
#%d [internal function]: {closur%s}()%A
#%d %s(%d): Swow\Coroutine::run(Object(Closure))
#%d {main}
  triggered in %swarning_in_coro_4.php on line %d

%ANotice: [Notice in R%d] running run-tests.php to test swow is deprecated
Stack trace:
#%d %swarning_in_coro_4.php(%d): trigger_error('running run-tes...', %d)
#%d [internal function]: {closur%s}()%A
#%d %s(%d): Swow\Coroutine::run(Object(Closure))
#%d {main}
  triggered in %swarning_in_coro_4.php on line %d

%AWarning: [Warning in R%d] running run-tests.php to test twosee is deprecated
Stack trace:
#%d %swarning_in_coro_4.php(%d): trigger_error('running run-tes...', %d)
#%d [internal function]: {closur%s}()%A
#%d %s(%d): Swow\Coroutine::run(Object(Closure))
#%d {main}
  triggered in %swarning_in_coro_4.php on line %d

%ADeprecated: [Deprecated in R%d] Passing E_USER_ERROR to trigger_error() is deprecated since 8.4, throw an exception or call exit with a string message instead
Stack trace:
#%d %swarning_in_coro_4.php(%d): trigger_error('running run-tes...', %d)
#%d [internal function]: {closur%s}()%A
#%d %s(%d): Swow\Coroutine::run(Object(Closure))
#%d {main}
  triggered in %swarning_in_coro_4.php on line %d

%AFatal error: [Fatal error in R%d] running run-tests.php to test dixyes is deprecated
Stack trace:
#%d %swarning_in_coro_4.php(%d): trigger_error('running run-tes...', %d)
#%d [internal function]: {closur%s}()%A
#%d %s(%d): Swow\Coroutine::run(Object(Closure))
#%d {main}
  triggered in %swarning_in_coro_4.php on line %d
