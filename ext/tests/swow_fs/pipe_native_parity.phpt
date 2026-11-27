--TEST--
swow_fs: pipes of commands give the same results as native PHP
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

/** Run PHP code in a child process, the way applications run shell commands */
function command(string $code): string
{
    return escapeshellarg(PHP_BINARY) . ' -n -r ' . escapeshellarg($code);
}

function probe(): array
{
    $results = [];

    $results['exec'] = [exec(command('echo "a\nb\n"; exit(3);'), $output, $code), $output, $code];
    ob_start();
    $last = system(command('echo "line 1\nline 2"; exit(4);'), $code);
    $results['system'] = [ob_get_clean(), $last, $code];
    ob_start();
    passthru(command('echo "binary\0data"; exit(5);'), $code);
    $results['passthru'] = [bin2hex(ob_get_clean()), $code];
    $results['shell_exec'] = shell_exec(command('echo "out"; fwrite(STDERR, "err");') . ' 2>&1');
    $results['shell_exec without output'] = shell_exec(command('exit(0);'));

    // a large output through every function
    $big = command('echo str_repeat("0123456789abcdef", 65536);');
    $results['big exec'] = md5(implode("\n", (function () use ($big) { exec($big, $o); return $o; })()));
    $results['big shell_exec'] = md5(shell_exec($big));

    // popen in both directions
    $pipe = popen(command('echo strtoupper(stream_get_contents(STDIN)); exit(6);'), 'r');
    $results['popen r'] = [fread($pipe, 1), stream_get_contents($pipe), pclose($pipe)];
    $file = tempnam(sys_get_temp_dir(), 'swow');
    $pipe = popen(command('file_put_contents($argv[1], strtoupper(stream_get_contents(STDIN))); exit(7);') . ' ' . escapeshellarg($file), 'w');
    $results['popen w'] = [fwrite($pipe, str_repeat('abc', 100000)), pclose($pipe), md5_file($file)];
    unlink($file);

    // proc_open: the parent writes more than a pipe buffer while the child reads it
    $process = proc_open([PHP_BINARY, '-n', '-r', 'echo md5(stream_get_contents(STDIN)), "\n"; fwrite(STDERR, "done");'], [
        0 => ['pipe', 'r'], 1 => ['pipe', 'w'], 2 => ['pipe', 'w'],
    ], $pipes);
    $payload = str_repeat('x', 2 * 1024 * 1024);
    $results['proc_open write'] = fwrite($pipes[0], $payload);
    fclose($pipes[0]);
    $results['proc_open read'] = [trim(fgets($pipes[1])) === md5($payload), stream_get_contents($pipes[2]), feof($pipes[1])];
    $results['proc_open close'] = proc_close($process);

    // a non-blocking pipe returns at once
    $process = proc_open([PHP_BINARY, '-n', '-r', 'usleep(300 * 1000); echo "late";'], [1 => ['pipe', 'w']], $pipes);
    stream_set_blocking($pipes[1], false);
    $start = microtime(true);
    $results['non-blocking read'] = [fread($pipes[1], 10), microtime(true) - $start < 0.2];
    stream_set_blocking($pipes[1], true);
    $results['blocking read after'] = stream_get_contents($pipes[1]);
    $results['meta'] = array_intersect_key(stream_get_meta_data($pipes[1]), ['stream_type' => 0, 'mode' => 0, 'seekable' => 0]);
    proc_close($process);

    return $results;
}

if (NativeBaseline::isNative()) {
    NativeBaseline::emit(probe());
}

$native = NativeBaseline::run(__FILE__);
$swow = probe();
Assert::same($swow, $native, 'native=' . json_encode($native) . ' swow=' . json_encode($swow));
echo json_encode(array_keys($swow)), "\n";
?>
--EXPECT--
["exec","system","passthru","shell_exec","shell_exec without output","big exec","big shell_exec","popen r","popen w","proc_open write","proc_open read","proc_open close","non-blocking read","blocking read after","meta"]
