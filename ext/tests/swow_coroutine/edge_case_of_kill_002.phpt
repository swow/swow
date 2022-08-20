--TEST--
swow_coroutine: edge case of kill
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

$worker = Coroutine::run(function () {
    while (true) {
        sleep(1);
    }
});

while (true) {
    $worker->resume();
}

?>
--EXPECTF--
%AFatal error: [Fatal error in R%d] Allowed memory size of %d bytes exhausted%A (tried to allocate %d bytes)
Stack trace:
#0 %s(%d): sleep(1)
#1 [internal function]: {closure}()
#2 %s(%d): Swow\Coroutine->resume()
#3 {main}
  triggered in %s on line %d
