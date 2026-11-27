--TEST--
swow_process: pclose() waits for the command without blocking the scheduler
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_win();
skip_if_function_not_exist('popen');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

/** Run PHP code in a child process, the way applications run shell commands */
function command(string $code): string
{
    return escapeshellarg(PHP_BINARY) . ' -n -r ' . escapeshellarg($code);
}

function probe(): array
{
    $results = [];

    // a writer feeds a command (gzip, sendmail, a log shipper) that keeps working after its input ends
    $file = tempnam(sys_get_temp_dir(), 'swow');
    $pipe = popen(command('$data = stream_get_contents(STDIN); usleep(100 * 1000); file_put_contents($argv[1], md5($data)); exit(3);') . ' ' . escapeshellarg($file), 'w');
    $payload = str_repeat('payload', 10000);
    $results['writer'] = [fwrite($pipe, $payload), pclose($pipe), file_get_contents($file) === md5($payload)];
    unlink($file);

    // a reader stops early while the command still runs
    $pipe = popen(command('echo "header\n"; fclose(STDOUT); usleep(100 * 1000); exit(4);'), 'r');
    $results['early close'] = [fgets($pipe), pclose($pipe)];

    // exec() of a command that closes its output before it exits
    $results['exec'] = [exec(command('echo "done"; fclose(STDOUT); usleep(100 * 1000); exit(5);'), $output, $code), $output, $code];

    // the shell killed by a signal: pclose() reports the raw wait status
    $pipe = popen('kill -9 $$', 'r');
    $results['killed'] = [stream_get_contents($pipe), pclose($pipe)];

    // a command that does not exist (the shell reports 126 or 127 depending on the platform)
    $pipe = popen('exec /nonexistent/swow-command 2>/dev/null', 'r');
    $results['not found'] = [stream_get_contents($pipe), pclose($pipe)];

    return $results;
}

if (NativeBaseline::isNative()) {
    NativeBaseline::emit(probe());
}

$native = NativeBaseline::run(__FILE__);
$swow = probe();
Assert::same($swow, $native, 'native=' . json_encode($native) . ' swow=' . json_encode($swow));
echo json_encode($swow), "\n";

$wait = 300;
SchedulerProbe::assertYields(static function () use ($wait): void {
    $pipe = popen(command("stream_get_contents(STDIN); usleep({$wait} * 1000);"), 'w');
    fwrite($pipe, 'input');
    Assert::same(pclose($pipe), 0);
}, $wait, 'pclose() of a writer');
SchedulerProbe::assertYields(static function () use ($wait): void {
    $pipe = popen(command("echo 'x'; fclose(STDOUT); usleep({$wait} * 1000);"), 'r');
    Assert::same(fgets($pipe), 'x');
    Assert::same(pclose($pipe), 0);
}, $wait, 'pclose() of a reader');
SchedulerProbe::assertYields(static fn () => exec(command("echo 'x'; fclose(STDOUT); usleep({$wait} * 1000);")), $wait, 'exec() of a command that closed its output');
echo "pclose() yields\n";

// file operations of other coroutines go on while a command is closed
$file = tempnam(sys_get_temp_dir(), 'swow');
$closed = false;
Coroutine::run(static function () use (&$closed): void {
    $pipe = popen(command('stream_get_contents(STDIN); usleep(500 * 1000);'), 'w');
    pclose($pipe);
    $closed = true;
});
for ($n = 0; $n < 10; $n++) {
    file_put_contents($file, str_repeat('x', 4096), FILE_APPEND);
}
Assert::same(filesize($file), 40960);
Assert::false($closed, 'file operations must not wait for pclose()');
unlink($file);
while (!$closed) {
    usleep(10 * 1000);
}
echo "file operations ran while pclose() waited\n";
?>
--EXPECTF--
{"writer":[70000,3,true],"early close":["header\n",4],"exec":["done",["done"],5],"killed":["",9],"not found":["",%d]}
pclose() yields
file operations ran while pclose() waited
