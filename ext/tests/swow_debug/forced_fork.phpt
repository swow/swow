--TEST--
swow_debug: forced observation continues independently after fork
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_extension_not_exist('pcntl');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Process\ChildProcess;

use function Swow\Debug\registerExtendedStatementHandler;

$calls = 0;
$handler = registerExtendedStatementHandler(static function () use (&$calls): void {
    $calls++;
}, true);
$observe = static function () use (&$calls): void {
    $before = $calls;
    $deadline = hrtime(true) + 1000000000;
    while ($calls - $before < 3 && hrtime(true) < $deadline) {
    }
    Assert::greaterThanEq($calls - $before, 3);
};
$observe();
$pid = pcntl_fork();
Assert::greaterThanEq($pid, 0);
if ($pid === 0) {
    $observe();
    $handler->remove();
    exit(0);
}
$observe();
Assert::same(pcntl_waitpid($pid, $status), $pid);
Assert::true(pcntl_wifexited($status));
Assert::same(pcntl_wexitstatus($status), 0);
$child = ChildProcess::fork(static function () use ($observe): int {
    $observe();
    return 0;
});
$observe();
Assert::same($child->wait(1000)->getExitCode(), 0);
$handler->remove();
echo "Done\n";
?>
--EXPECT--
Done
