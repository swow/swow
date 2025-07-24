--TEST--
swow_coroutine: edge case of kill
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';

if (memory_get_usage() === 0) {
    // zend mm not enabled, skip test
    exit('SKIP: zend mm not enabled');
}
?>
--INI--
memory_limit=32M
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

$worker = Coroutine::run(static function (): void {
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
#1 [internal function]: {closur%s}()
#2 %s(%d): Swow\Coroutine->resume()
#3 {main}
  triggered in %s on line %d
