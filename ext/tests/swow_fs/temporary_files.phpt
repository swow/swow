--TEST--
swow_fs: temporary file lifetime and file wrapper metadata
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!is_writable(sys_get_temp_dir()), 'temporary directory is not writable');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$first = $second = $spill = null;
$filename = sys_get_temp_dir() . '/swow-wrapper-' . getRandomBytes(12);
try {
    $first = tmpfile();
    $second = tmpfile();
    Assert::true(is_resource($first));
    Assert::true(is_resource($second));
    $firstPath = stream_get_meta_data($first)['uri'];
    $secondPath = stream_get_meta_data($second)['uri'];
    Assert::notSame($firstPath, $secondPath);
    Assert::true(file_exists($firstPath));
    Assert::same(fwrite($first, "first\0body"), 10);
    Assert::same(fwrite($second, 'second'), 6);
    Assert::true(rewind($first));
    Assert::same(stream_get_contents($first), "first\0body");
    Assert::true(rewind($second));
    Assert::same(stream_get_contents($second), 'second');
    Assert::true(fclose($first));
    $first = null;
    clearstatcache(true, $firstPath);
    Assert::false(file_exists($firstPath));
    Assert::true(file_exists($secondPath));
    Assert::true(fclose($second));
    $second = null;
    clearstatcache(true, $secondPath);
    Assert::false(file_exists($secondPath));

    // Exceed the memory limit so the temporary stream uses a real backing file.
    $spill = fopen('php://temp/maxmemory:32', 'w+b');
    $body = str_repeat("a\0b", 128);
    Assert::same(fwrite($spill, $body), strlen($body));
    Assert::same(fstat($spill)['size'], strlen($body));
    Assert::true(rewind($spill));
    Assert::same(stream_get_contents($spill), $body);
    Assert::true(ftruncate($spill, 3));
    Assert::true(rewind($spill));
    Assert::same(stream_get_contents($spill), "a\0b");

    $uri = 'file://' . $filename;
    $modified = time() - 60;
    Assert::true(touch($uri, $modified, $modified));
    Assert::true(file_exists($filename));
    Assert::true(chmod($uri, 0600));
    clearstatcache(true, $filename);
    Assert::same(filemtime($filename), $modified);
    Assert::same(file_put_contents($uri, 'wrapper'), 7);
    Assert::same(file_get_contents($uri), 'wrapper');
    Assert::same(filesize($uri), 7);
    Assert::true(unlink($uri));
    clearstatcache(true, $filename);
    Assert::false(file_exists($filename));
} finally {
    foreach ([$first, $second, $spill] as $stream) {
        if (is_resource($stream)) {
            fclose($stream);
        }
    }
    if (file_exists($filename)) {
        unlink($filename);
    }
}
echo "Done\n";
?>
--EXPECT--
Done
