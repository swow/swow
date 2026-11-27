--TEST--
swow_fs: pipes wait on the event loop instead of occupying the thread pool
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_win();
skip_if_function_not_exist('proc_open');
skip_if_function_not_exist('popen');
?>
--INI--
swow.async_threads=2
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Sync\WaitReference;

function command(string $code): string
{
    return escapeshellarg(PHP_BINARY) . ' -n -r ' . escapeshellarg($code);
}

/*
 * A web worker runs slow commands (a converter, a probe) for several requests
 * at once, while other requests read and write files. With two threads in the
 * pool, the commands used to hold both threads until they exited, and every
 * file operation waited behind them.
 */
$commands = 6;
$wr = new WaitReference();
$results = [];
for ($i = 0; $i < $commands; $i++) {
    Coroutine::run(static function () use ($i, $wr, &$results): void {
        $results[$i] = trim(shell_exec(command("usleep(500 * 1000); echo {$i};")));
    });
}
$file = tempnam(sys_get_temp_dir(), 'swow');
$finishedCommandsAfterFileOperations = null;
SchedulerProbe::measure(static function () use ($file, &$results, &$finishedCommandsAfterFileOperations): void {
    usleep(50 * 1000);
    for ($n = 0; $n < 10; $n++) {
        file_put_contents($file, str_repeat('x', 4096), FILE_APPEND);
        Assert::same(strlen(file_get_contents($file)), 4096 * ($n + 1));
    }
    $finishedCommandsAfterFileOperations = count($results);
});
unlink($file);
WaitReference::wait($wr);
ksort($results);
Assert::same($results, array_map('strval', range(0, $commands - 1)));
// ordering rather than elapsed time, which depends on how loaded the machine is
Assert::same($finishedCommandsAfterFileOperations, 0, 'file operations must not wait for the commands');
echo "file operations ran while the commands were waiting\n";

// every way of running a command lets the other coroutines run
$wait = 300;
$sleep = "usleep({$wait} * 1000);";
SchedulerProbe::assertYields(static fn () => exec(command($sleep . 'echo "x";')), $wait, 'exec()');
SchedulerProbe::assertYields(static fn () => shell_exec(command($sleep . 'echo "x";')), $wait, 'shell_exec()');
SchedulerProbe::assertYields(static function () use ($sleep): void {
    ob_start();
    system(command($sleep . 'echo "x";'));
    ob_end_clean();
}, $wait, 'system()');
SchedulerProbe::assertYields(static function () use ($sleep): void {
    ob_start();
    passthru(command($sleep . 'echo "x";'));
    ob_end_clean();
}, $wait, 'passthru()');
SchedulerProbe::assertYields(static function () use ($sleep): string {
    $pipe = popen(command($sleep . 'echo "x";'), 'r');
    $data = stream_get_contents($pipe);
    pclose($pipe);
    return $data;
}, $wait, 'popen() read');
SchedulerProbe::assertYields(static function () use ($sleep): void {
    $process = proc_open([PHP_BINARY, '-n', '-r', $sleep . 'echo "x";'], [1 => ['pipe', 'w']], $pipes);
    stream_get_contents($pipes[1]);
    proc_close($process);
}, $wait, 'proc_open() read');
// a child that reads its input slowly makes the writer wait for buffer space
SchedulerProbe::assertYields(static function () use ($wait): void {
    $process = proc_open([PHP_BINARY, '-n', '-r', "usleep({$wait} * 1000); stream_get_contents(STDIN);"], [0 => ['pipe', 'r']], $pipes);
    Assert::same(fwrite($pipes[0], str_repeat('y', 1024 * 1024)), 1024 * 1024);
    fclose($pipes[0]);
    proc_close($process);
}, $wait, 'a write to a slow reader');
echo "every command function yields\n";

/*
 * A second coroutine that reads the same pipe stream while the first one waits
 * is refused, as with a socket: the stream buffer belongs to the first reader.
 * The first reader is not disturbed and gets every line.
 */
$process = proc_open([PHP_BINARY, '-n', '-r', 'for ($i = 0; $i < 40; $i++) { echo "job {$i}\n"; usleep(5000); }'], [1 => ['pipe', 'w']], $pipes);
$jobs = [];
$reader = Coroutine::run(static function () use ($pipes, &$jobs): void {
    while (($line = fgets($pipes[1])) !== false) {
        $jobs[] = trim($line);
    }
});
$second = null;
Coroutine::run(static function () use ($pipes, &$second): void {
    $line = @fgets($pipes[1]);
    $second = [$line, error_get_last()['message'] ?? ''];
});
while ($reader->isAlive()) {
    usleep(10 * 1000);
}
proc_close($process);
Assert::same($jobs, array_map(static fn (int $i): string => "job {$i}", range(0, 39)));
echo "first reader: ", count($jobs), " jobs\n";
var_dump($second[0], str_contains($second[1], 'Descriptor is already being read by another coroutine'));
?>
--EXPECT--
file operations ran while the commands were waiting
every command function yields
first reader: 40 jobs
bool(false)
bool(true)
