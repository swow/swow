--TEST--
swow_fs: file metadata, locking, synchronization and directory rewind
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!is_writable(sys_get_temp_dir()), 'temporary directory is not writable');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$directory = sys_get_temp_dir() . '/swow_fs_' . getRandomBytes(12);
$filename = $directory . '/source';
$copy = $directory . '/copy';
$first = $second = $entries = null;
try {
    Assert::true(mkdir($directory));
    Assert::same(file_put_contents($filename, "first\nsecond\n"), 13);
    Assert::true(copy($filename, $copy));
    Assert::same(file_get_contents($copy), "first\nsecond\n");
    Assert::true(chmod($copy, 0600));
    $modified = time() - 60;
    Assert::true(touch($copy, $modified, $modified));
    clearstatcache(true, $copy);
    Assert::same(filemtime($copy), $modified);
    Assert::same(stat($copy)['size'], 13);
    Assert::same(lstat($copy)['size'], 13);

    $first = fopen($filename, 'r+b');
    $second = fopen($filename, 'r+b');
    Assert::same(fgets($first), "first\n");
    Assert::same(fgetc($first), 's');
    Assert::true(flock($first, LOCK_EX));
    $wouldBlock = 0;
    Assert::false(flock($second, LOCK_EX | LOCK_NB, $wouldBlock));
    Assert::same($wouldBlock, 1);
    Assert::true(flock($first, LOCK_UN));
    Assert::true(flock($second, LOCK_EX | LOCK_NB));
    Assert::true(flock($second, LOCK_UN));
    Assert::true(rewind($first));
    Assert::same(fwrite($first, 'FIRST'), 5);
    Assert::true(fflush($first));
    Assert::true(fsync($first));
    Assert::true(fdatasync($first));
    Assert::true(rewind($second));
    Assert::same(stream_get_contents($second), "FIRST\nsecond\n");

    $entries = opendir($directory);
    /** @param resource $handle */
    $readEntries = static function (mixed $handle): array {
        $names = [];
        while (($name = readdir($handle)) !== false) {
            $names[] = $name;
        }
        sort($names);
        return $names;
    };
    Assert::same($readEntries($entries), ['.', '..', 'copy', 'source']);
    rewinddir($entries);
    Assert::same($readEntries($entries), ['.', '..', 'copy', 'source']);
} finally {
    foreach ([$first, $second] as $stream) {
        if (is_resource($stream)) {
            fclose($stream);
        }
    }
    if (is_resource($entries)) {
        closedir($entries);
    }
    foreach ([$filename, $copy] as $path) {
        if (file_exists($path)) {
            unlink($path);
        }
    }
    if (is_dir($directory)) {
        rmdir($directory);
    }
}

echo "Done\n";
?>
--EXPECT--
Done
