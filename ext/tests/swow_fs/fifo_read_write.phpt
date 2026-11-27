--TEST--
swow_fs: a FIFO is read and written while other coroutines run
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
require __DIR__ . '/../include/bootstrap.php';
skip_if_win();
skip_if_cannot_create_fifo();
?>
--INI--
ffi.enable=1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

/*
 * Two parts of an application talk through a named pipe. Pipes wait on the event loop,
 * but some systems cannot poll every kind of descriptor (macOS refuses a FIFO opened
 * with fopen()), and such a pipe must still work and let the other coroutines run.
 */
$path = make_fifo();
$payload = str_repeat('0123456789', 100000);
$writer = Coroutine::run(static function () use ($path, $payload): void {
    $w = fopen($path, 'w');
    usleep(200 * 1000);
    Assert::same(fwrite($w, "first line\n"), 11);
    Assert::same(fwrite($w, $payload), strlen($payload));
    fclose($w);
});
$r = fopen($path, 'r');
$line = SchedulerProbe::assertYields(static fn() => fgets($r), 200, 'fgets() on a FIFO');
var_dump($line);
Assert::same(md5(stream_get_contents($r)), md5($payload));
fclose($r);
unlink($path);
echo "Done\n";
?>
--EXPECT--
string(11) "first line
"
Done
