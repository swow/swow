--TEST--
swow_process: pcntl_wait() and pcntl_waitpid() wait without blocking the scheduler
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_win();
skip_if_extension_not_exist('pcntl');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Sync\WaitReference;

/** Fork a worker that sleeps, then exits with the given code */
function forkWorker(int $sleepMs, int $exitCode): int
{
    $pid = pcntl_fork();
    Assert::greaterThanEq($pid, 0);
    if ($pid === 0) {
        usleep($sleepMs * 1000);
        exitWorker($exitCode);
    }
    return $pid;
}

/** Replace the child with a shell, so the parent's state inherited by the child is not shut down twice */
function exitWorker(int $exitCode): void
{
    if ($exitCode < 0) {
        // a negative code asks the child to die by that signal, as a crashed worker does
        pcntl_exec('/bin/sh', ['-c', 'kill -' . -$exitCode . ' $$']);
    }
    pcntl_exec('/bin/sh', ['-c', "exit {$exitCode}"]);
    exit(127);
}

function probe(): array
{
    $results = [];

    $pid = forkWorker(50, 3);
    $status = 'untouched';
    $results['waitpid'] = [pcntl_waitpid($pid, $status) === $pid, pcntl_wifexited($status), pcntl_wexitstatus($status)];

    $pid = forkWorker(50, -9);
    $results['waitpid signaled'] = [pcntl_waitpid($pid, $status) === $pid, pcntl_wifsignaled($status), pcntl_wtermsig($status)];

    $pid = forkWorker(50, 4);
    $rusage = 'untouched';
    $results['wait with rusage'] = [pcntl_wait($status, 0, $rusage) === $pid, pcntl_wexitstatus($status), array_keys($rusage)];

    $pid = forkWorker(200, 5);
    $results['WNOHANG on a running child'] = pcntl_waitpid($pid, $status, WNOHANG);
    $results['then blocking'] = [pcntl_waitpid($pid, $status, 0, $rusage) === $pid, pcntl_wexitstatus($status), count($rusage)];

    // no child left: the status keeps the value that was passed in, the error is recorded
    $status = 7;
    $results['no child'] = [pcntl_waitpid(-1, $status), $status, pcntl_get_last_error() === PCNTL_ECHILD];
    $status = 8;
    $results['wait without child'] = [pcntl_wait($status), $status, pcntl_get_last_error() === PCNTL_ECHILD];
    $results['unknown pid'] = [pcntl_waitpid(999999, $status), pcntl_get_last_error() === PCNTL_ECHILD];

    return $results;
}

if (NativeBaseline::isNative()) {
    NativeBaseline::emit(probe());
}

$native = NativeBaseline::run(__FILE__, ['pcntl']);
$swow = probe();
Assert::same($swow, $native, 'native=' . json_encode($native) . ' swow=' . json_encode($swow));
echo json_encode($swow), "\n";

// a master process waits for one of its workers while its other coroutines keep serving
$wait = 300;
$pid = forkWorker($wait, 0);
SchedulerProbe::assertYields(static fn() => Assert::same(pcntl_waitpid($pid, $status), $pid), $wait, 'pcntl_waitpid()');
$pid = forkWorker($wait, 0);
SchedulerProbe::assertYields(static fn() => Assert::same(pcntl_wait($status), $pid), $wait, 'pcntl_wait()');
echo "the waits yield\n";

/*
 * A supervisor waits for each worker in its own coroutine. Every coroutine gets the exit
 * code of its own worker, and the workers are reaped in the order they exit, not in the
 * order the coroutines started waiting.
 */
$workers = [[300, 11], [100, 12], [200, 13]];
$reaped = [];
$wr = new WaitReference();
foreach ($workers as [$sleepMs, $exitCode]) {
    $pid = forkWorker($sleepMs, $exitCode);
    Coroutine::run(static function () use ($pid, $exitCode, $wr, &$reaped): void {
        Assert::same(pcntl_waitpid($pid, $status), $pid);
        Assert::same(pcntl_wexitstatus($status), $exitCode);
        $reaped[] = $exitCode;
    });
}
WaitReference::wait($wr);
Assert::same($reaped, [12, 13, 11]);
echo 'reaped in exit order: ', json_encode($reaped), "\n";

// a coroutine waiting for any child while another waits for a particular one
$first = forkWorker(100, 21);
$second = forkWorker(250, 22);
$byPid = null;
$coroutine = Coroutine::run(static function () use ($second, &$byPid): void {
    $byPid = [pcntl_waitpid($second, $status), pcntl_wexitstatus($status)];
});
$any = [pcntl_wait($status), pcntl_wexitstatus($status)];
while ($coroutine->isAlive()) {
    usleep(10 * 1000);
}
Assert::same($any, [$first, 21]);
Assert::same($byPid, [$second, 22]);
echo "any child and a particular child: ok\n";

// a SIGCHLD handler installed with pcntl_signal() keeps working, the wait is native then
pcntl_async_signals(false);
$signals = 0;
pcntl_signal(SIGCHLD, static function () use (&$signals): void {
    $signals++;
});
$pid = forkWorker(50, 31);
Assert::same(pcntl_waitpid($pid, $status), $pid);
Assert::same(pcntl_wexitstatus($status), 31);
pcntl_signal_dispatch();
Assert::same($signals, 1);
pcntl_signal(SIGCHLD, SIG_DFL);
// back to the default disposition, the wait yields again
$pid = forkWorker($wait, 0);
SchedulerProbe::assertYields(static fn() => Assert::same(pcntl_waitpid($pid, $status), $pid), $wait, 'pcntl_waitpid() after SIG_DFL');
echo "pcntl_signal(SIGCHLD): ok\n";
?>
--EXPECT--
{"waitpid":[true,true,3],"waitpid signaled":[true,true,9],"wait with rusage":[true,4,["ru_oublock","ru_inblock","ru_msgsnd","ru_msgrcv","ru_maxrss","ru_ixrss","ru_idrss","ru_minflt","ru_majflt","ru_nsignals","ru_nvcsw","ru_nivcsw","ru_nswap","ru_utime.tv_usec","ru_utime.tv_sec","ru_stime.tv_usec","ru_stime.tv_sec"]],"WNOHANG on a running child":0,"then blocking":[true,5,17],"no child":[-1,7,true],"wait without child":[-1,8,true],"unknown pid":[-1,true]}
the waits yield
reaped in exit order: [12,13,11]
any child and a particular child: ok
pcntl_signal(SIGCHLD): ok
