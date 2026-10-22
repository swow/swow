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
--EXPECTREGEX--
(?![\s\S]*(?:Never here|Deadlock|process timed out))(?:Fatal error: \[Fatal error in (?:R\d+|main)\] Allowed memory size of \d+ bytes exhausted[^\n]* \(tried to allocate \d+ bytes\)
Stack trace:
(?:#0 [^\n]+\(\d+\): sleep\(1\)
#1 \[internal function\]: \{closur[^\n]*\}\(\)
#2 [^\n]+\(\d+\): Swow\\Coroutine->resume\(\)
#3 \{main\}|#0 [^\n]+\(\d+\): Swow\\Coroutine->resume\(\)
#1 \{main\})
  triggered in [^\n]+edge_case_of_kill_002\.php on line \d+|Fatal error: Allowed memory size of \d+ bytes exhausted[^\n]* \(tried to allocate \d+ bytes\) in [^\n]+edge_case_of_kill_002\.php on line \d+)
