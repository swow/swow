--TEST--
swow_fs: disabled asynchronous file hooks preserve PHP file and directory operations
--INI--
swow.async_file=0
swow.async_tty=0
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!is_writable(sys_get_temp_dir()), 'temporary directory is not writable');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

Assert::same(ini_get('swow.async_file'), '0');
Assert::same(ini_get('swow.async_tty'), '0');
$directory = sys_get_temp_dir() . '/swow-sync-file-' . getRandomBytes(12);
$source = $directory . '/source';
$copy = $directory . '/copy';
$renamed = $directory . '/renamed';
$stream = $entries = null;
try {
    Assert::true(mkdir($directory));
    Assert::same(file_put_contents($source, "first\nsecond\n"), 13);
    $stream = fopen($source, 'r+b');
    Assert::same(fgets($stream), "first\n");
    Assert::same(ftell($stream), 6);
    Assert::same(fseek($stream, -7, SEEK_END), 0);
    Assert::same(fwrite($stream, 'SECOND'), 6);
    Assert::true(fflush($stream));
    Assert::same(fstat($stream)['size'], 13);
    Assert::true(rewind($stream));
    Assert::same(stream_get_contents($stream), "first\nSECOND\n");
    Assert::true(fclose($stream));
    $stream = null;
    Assert::true(copy($source, $copy));
    Assert::true(chmod($copy, 0600));
    $modified = time() - 60;
    Assert::true(touch($copy, $modified, $modified));
    Assert::same(filemtime($copy), $modified);
    Assert::same(stat($copy)['size'], 13);
    Assert::true(rename($copy, $renamed));
    Assert::same(file_get_contents($renamed), "first\nSECOND\n");
    $entries = opendir($directory);
    $names = [];
    while (($name = readdir($entries)) !== false) {
        $names[] = $name;
    }
    sort($names);
    Assert::same($names, ['.', '..', 'renamed', 'source']);
    rewinddir($entries);
    Assert::notSame(readdir($entries), false);
    Assert::true(unlink($source));
    Assert::true(unlink($renamed));
    closedir($entries);
    $entries = null;
    Assert::true(rmdir($directory));
} finally {
    if (is_resource($stream)) {
        fclose($stream);
    }
    if (is_resource($entries)) {
        closedir($entries);
    }
    foreach ([$source, $copy, $renamed] as $path) {
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
