--TEST--
swow_process: proc_open socket descriptors do not block the scheduler
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_win('socket descriptors of proc_open are POSIX only here');
skip_if_function_not_exist('proc_open');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Sync\WaitReference;

/** A worker that reads a job from stdin and streams its progress to stdout slowly */
function startWorker(int $id)
{
    $code = <<<'PHP'
        $job = trim(fgets(STDIN));
        for ($step = 1; $step <= 3; $step++) {
            usleep(100 * 1000);
            echo "{$job} step {$step}\n";
        }
        exit(0);
        PHP;
    $process = proc_open([PHP_BINARY, '-n', '-r', $code], [0 => ['socket'], 1 => ['socket']], $pipes);
    Assert::true(is_resource($process));
    fwrite($pipes[0], "job {$id}\n");
    return [$process, $pipes];
}

function probe(): array
{
    $results = [];
    [$process, $pipes] = startWorker(0);
    $results['stream_type'] = stream_get_meta_data($pipes[1])['stream_type'];
    $results['output'] = stream_get_contents($pipes[1]);
    $results['exit'] = proc_close($process);
    return $results;
}

if (NativeBaseline::isNative()) {
    NativeBaseline::emit(probe());
}

$native = NativeBaseline::run(__FILE__);
$swow = probe();
Assert::same($swow, $native, 'native=' . json_encode($native) . ' swow=' . json_encode($swow));
echo json_encode($swow), "\n";

// a supervisor reads the progress of several workers at the same time
$workers = 4;
$outputs = [];
$measured = SchedulerProbe::measure(static function () use ($workers, &$outputs): void {
    $wr = new WaitReference();
    for ($id = 0; $id < $workers; $id++) {
        Coroutine::run(static function () use ($id, $wr, &$outputs): void {
            [$process, $pipes] = startWorker($id);
            while (($line = fgets($pipes[1])) !== false) {
                $outputs[$id][] = trim($line);
            }
            Assert::same(proc_close($process), 0);
        });
    }
    WaitReference::wait($wr);
});
ksort($outputs);
foreach ($outputs as $id => $lines) {
    Assert::same($lines, ["job {$id} step 1", "job {$id} step 2", "job {$id} step 3"]);
}
// one worker takes about 300ms; serial reads would take over 1.2s
Assert::lessThan($measured['elapsed_ms'], 1000, 'the workers must be read concurrently');
Assert::greaterThan($measured['ticks'], 10, 'the ticker must run while the supervisor waits');
echo "workers: ", count($outputs), "\n";
?>
--EXPECT--
{"stream_type":"generic_socket","output":"job 0 step 1\njob 0 step 2\njob 0 step 3\n","exit":0}
workers: 4
