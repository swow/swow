--TEST--
swow_coroutine: dead lock
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
/* TODO: replace it */
skip_if_constant_not_defined('SIGTERM');
skip_if_function_not_exist('pcntl_fork');
skip_if_function_not_exist('pcntl_wait');
skip_if_function_not_exist('posix_kill');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

foreach (
    [
        function () {
            Swow\Coroutine::yield();
            echo 'Never here' . PHP_LF;
        },
        function () {
            Swow\Coroutine::run(function () {
                Swow\Coroutine::yield();
                echo 'Never here' . PHP_LF;
            });
        }
    ] as $main
) {
    $child = pcntl_fork();
    if ($child < 0) {
        return;
    }
    if ($child === 0) {
        return $main();
    } else {
        switchProcess();
        posix_kill($child, SIGTERM);
        switchProcess();
        pcntl_wait($status);
        Assert::same($status, SIGTERM);
    }
}
echo PHP_LF . 'Done' . PHP_LF;

?>
--EXPECTF--
Warning: [Warning in R0] Dead lock: all coroutines are asleep in Unknown on line 0

Warning: [Warning in R0] Dead lock: all coroutines are asleep in Unknown on line 0

Done
