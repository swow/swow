--TEST--
swow_base: check swow.async_file work
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_extension_not_exist('posix');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$fifoName = __DIR__ . '/ini_async_file__fifo';
@unlink($fifoName);
Assert::true(posix_mkfifo($fifoName, 0600));

$f = fopen($fifoName, 'r+');

$proc = proc_open([
    test_php_path(),
    ...php_options_with_swow(),
    '-d', 'swow.async_file = 0',
    '-d', 'swow.async_tty = 0',
    __DIR__ . '/ini_async_file_fifo_child.inc',
    $fifoName,
], [
    1 => ['pipe', 'w'],
    2 => STDERR,
], $pipes);
Assert::notSame($proc, false);

while (true) {
    $line = fgets($pipes[1]);
    if ($line === "read-end read\n") {
        break;
    }
    echo $line;
}

printf("write-end write\n");
fwrite($f, 'hello');

// read-end read: hello must before Done
$line = fgets($pipes[1]);
Assert::same($line, "read-end read: hello\n");
$line = fgets($pipes[1]);
Assert::same($line, "Done\n");

proc_close($proc);

echo "Done\n";
?>
--CLEAN--
<?php
$fifoName = __DIR__ . '/ini_async_file__fifo';
@unlink($fifoName);
?>
--EXPECT--
child process started
write-end write
Done
