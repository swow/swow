--TEST--
swow_fs: file contents, offsets, metadata and directory operations
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!is_writable(sys_get_temp_dir()), 'temporary directory is not writable');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$directory = sys_get_temp_dir() . '/swow_fs_' . getRandomBytes(12);
$filename = $directory . '/contents';
$renamed = $directory . '/renamed';
$stream = null;
try {
    Assert::true(mkdir($directory));
    Assert::true(is_dir($directory));
    Assert::same(file_put_contents($filename, 'abcdef'), 6);
    Assert::same(file_put_contents($filename, 'gh', FILE_APPEND), 2);
    Assert::same(file_get_contents($filename, offset: 2, length: 3), 'cde');
    clearstatcache(true, $filename);
    Assert::same(filesize($filename), 8);
    Assert::true(is_file($filename));
    Assert::true(is_readable($filename));
    Assert::true(is_writable($filename));
    $stream = fopen($filename, 'r+b');
    Assert::same(fread($stream, 2), 'ab');
    Assert::same(ftell($stream), 2);
    Assert::same(fseek($stream, -2, SEEK_END), 0);
    Assert::same(fread($stream, 2), 'gh');
    Assert::true(rewind($stream));
    Assert::same(fwrite($stream, 'AB'), 2);
    Assert::true(fflush($stream));
    Assert::true(ftruncate($stream, 4));
    Assert::same(fstat($stream)['size'], 4);
    Assert::true(rewind($stream));
    Assert::same(stream_get_contents($stream), 'ABcd');
    Assert::same(fread($stream, 1), '');
    Assert::true(feof($stream));
    Assert::true(fclose($stream));
    $stream = null;
    Assert::true(rename($filename, $renamed));
    Assert::same(scandir($directory), ['.', '..', 'renamed']);
    Assert::same(file($renamed), ['ABcd']);
    Assert::same(realpath($renamed), realpath($directory) . '/renamed');
    Assert::same(file_get_contents($renamed), 'ABcd');
    Assert::true(unlink($renamed));
    clearstatcache(true, $renamed);
    Assert::false(file_exists($renamed));
    Assert::true(rmdir($directory));
} finally {
    if (is_resource($stream)) {
        fclose($stream);
    }
    foreach ([$filename, $renamed] as $path) {
        if (is_file($path)) {
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
