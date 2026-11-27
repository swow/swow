--TEST--
swow_process: proc_close after proc_get_status matches native PHP
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_win();
skip_if_function_not_exist('proc_open');
skip_if_function_not_exist('popen');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

/**
 * A process supervisor polls proc_get_status() until the worker is gone and
 * closes it afterwards. proc_get_status() reaps the child, so proc_close() has
 * to report the exit code that PHP cached for it (PHP >= 8.3).
 */
function probe(): array
{
    $waitUntilStopped = static function ($process): array {
        do {
            $status = proc_get_status($process);
            if ($status['running']) {
                usleep(10 * 1000);
            }
        } while ($status['running']);
        return $status;
    };
    $results = [];
    foreach ([0, 4, 255] as $code) {
        $process = proc_open([PHP_BINARY, '-n', '-r', "exit({$code});"], [], $pipes);
        $status = $waitUntilStopped($process);
        $results["exit {$code}"] = ['exitcode' => $status['exitcode'], 'proc_close' => proc_close($process)];
    }

    $process = proc_open([PHP_BINARY, '-n', '-r', 'sleep(10);'], [], $pipes);
    usleep(100 * 1000);
    proc_terminate($process, 15);
    $status = $waitUntilStopped($process);
    $results['terminated'] = [
        'signaled' => $status['signaled'],
        'termsig' => $status['termsig'],
        'proc_close' => proc_close($process),
    ];

    $process = proc_open([PHP_BINARY, '-n', '-r', 'usleep(100 * 1000); exit(3);'], [], $pipes);
    $results['closed while running'] = ['proc_close' => proc_close($process)];

    return $results;
}

if (NativeBaseline::isNative()) {
    NativeBaseline::emit(probe());
}

$native = NativeBaseline::run(__FILE__);
$swow = probe();
Assert::same($swow, $native, 'native=' . json_encode($native) . ' swow=' . json_encode($swow));
foreach ($swow as $case => $result) {
    echo $case, ': ', json_encode($result), "\n";
}
?>
--EXPECTF--
exit 0: {"exitcode":0,"proc_close":%i}
exit 4: {"exitcode":4,"proc_close":%i}
exit 255: {"exitcode":255,"proc_close":%i}
terminated: {"signaled":true,"termsig":15,"proc_close":-1}
closed while running: {"proc_close":3}
