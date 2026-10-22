--TEST--
swow_coroutine: OOM terminates the request according to the native PHP policy
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
if (memory_get_usage() === 0) {
    skip('Zend memory manager is required');
}
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$allocation = '$first = str_repeat("X", 6 * 1024 * 1024); $second = str_repeat("Y", 6 * 1024 * 1024); echo "Unexpected completion\n";';
$results = [];
foreach (['native', 'coroutine', 'defer'] as $mode) {
    $coroutines = $mode !== 'native';
    $code = $coroutines ? <<<'PHP'
        use Swow\Coroutine;
        register_shutdown_function(static function (): void {
            echo "shutdown: ", Coroutine::count(), "\n";
        });
        for ($i = 0; $i < 2; $i++) {
            Coroutine::run(static function (): void {
                Coroutine::yield();
                echo "Unexpected resumption\n";
            });
        }
        Coroutine::run(static function (): void {
        PHP : 'register_shutdown_function(static function (): void { echo "shutdown\n"; });';
    $code .= $mode === 'defer'
        ? 'Swow\defer(static function (): void {' . $allocation . '}); throw new RuntimeException("outer");'
        : $allocation;
    if ($coroutines) {
        $code .= '}); Swow\Sync\waitAll(); echo "Unexpected continuation\n";';
    }
    $output = '';
    $status = php_proc_with_swow(
        ['-d', 'swow.enable=' . (int) $coroutines, '-d', 'memory_limit=8M', '-r', $code],
        static function ($process, array $pipes) use (&$output): void {
            $output = stream_get_contents($pipes[1]);
            fclose($pipes[1]);
        },
        ['stderr' => ['redirect', 1]],
    );
    Assert::notSame($status, 0);
    Assert::contains($output, 'Allowed memory size of');
    Assert::false(str_contains($output, 'Unexpected'));
    Assert::false(str_contains($output, 'Deadlock'));
    $results[] = [$status, $output];
}

// Security backports can use _exit() without running any PHP shutdown code.
// A graceful native OOM must retain Swow's fatal context and reach shutdown;
// either policy must terminate the entire request rather than only its coroutine.
foreach (array_slice($results, 1) as [$status, $output]) {
    Assert::same($status, $results[0][0]);
    if (str_contains($results[0][1], 'shutdown')) {
        Assert::contains($output, '[Fatal error in R');
        Assert::contains($output, 'shutdown: 4');
    } else {
        Assert::false(str_contains($output, 'shutdown'));
    }
}
echo "Done\n";
?>
--EXPECT--
Done
