--TEST--
swow_misc: trig user warning in coroutine
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';
use Swow\Coroutine;

Coroutine::run(function () {
    // E_PARSE
    eval(/* @lang text */ "i'm a bad expression");
});

Coroutine::run(function () {
    trigger_error('running run-tests.php to test php is deprecated', E_USER_DEPRECATED);
    trigger_error('running run-tests.php to test swow is deprecated', E_USER_NOTICE);
    trigger_error('running run-tests.php to test twosee is deprecated', E_USER_WARNING);
    trigger_error('running run-tests.php to test dixyes is deprecated', E_USER_ERROR);
});

echo "Never here\n";
?>
--EXPECTF--
%A: [Parse error in R%d] %s in %swarning_in_coro.php(%d) : eval()'d code on line 1

%ADeprecated: [Deprecated in R%d] running run-tests.php to test php is deprecated
Stack trace:
#0 %swarning_in_coro.php(%d): trigger_error('running run-tes...', %d)
#1 [internal function]: {closure}()
#2 {main}
  triggered in %swarning_in_coro.php on line %d

%ANotice: [Notice in R%d] running run-tests.php to test swow is deprecated
Stack trace:
#0 %swarning_in_coro.php(%d): trigger_error('running run-tes...', %d)
#1 [internal function]: {closure}()
#2 {main}
  triggered in %swarning_in_coro.php on line %d

%AWarning: [Warning in R%d] running run-tests.php to test twosee is deprecated
Stack trace:
#0 %swarning_in_coro.php(%d): trigger_error('running run-tes...', %d)
#1 [internal function]: {closure}()
#2 {main}
  triggered in %swarning_in_coro.php on line %d

%AFatal error: [Fatal error in R%d] running run-tests.php to test dixyes is deprecated
Stack trace:
#0 %swarning_in_coro.php(%d): trigger_error('running run-tes...', %d)
#1 [internal function]: {closure}()
#2 {main}
  triggered in %swarning_in_coro.php on line %d
