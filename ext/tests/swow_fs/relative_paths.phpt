--TEST--
swow_fs: relative UTF-8 paths, recursive directories and metadata
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!is_writable(sys_get_temp_dir()), 'temporary directory is not writable');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$originalDirectory = getcwd();
$directory = sys_get_temp_dir() . '/swow-relative-' . getRandomBytes(12);
$filename = 'file-café.txt';
$stream = null;
try {
    Assert::true(mkdir($directory));
    Assert::true(chdir($directory));
    Assert::true(mkdir('nested/child', 0700, true));
    Assert::true(chdir('nested/./child/..'));
    $stream = fopen('./child/../' . $filename, 'x+b');
    Assert::same(fwrite($stream, "relative\0body"), 13);
    Assert::true(fflush($stream));
    Assert::same(fstat($stream)['size'], 13);
    Assert::same(file_get_contents($filename), "relative\0body");
    Assert::true(fclose($stream));
    $stream = null;
    $modified = time() - 120;
    Assert::true(touch($filename, $modified, $modified));
    Assert::true(chmod($filename, 0600));
    clearstatcache(true, $filename);
    Assert::same(filemtime($filename), $modified);
    Assert::same(filesize($filename), 13);
    Assert::same(realpath('./child/../' . $filename), realpath('.') . DIRECTORY_SEPARATOR . $filename);
    Assert::true(rename($filename, './child/renamed'));
    Assert::same(file_get_contents('child/renamed'), "relative\0body");
    Assert::false(file_exists($filename));
    Assert::true(unlink('child/renamed'));
    Assert::true(rmdir('child'));
    Assert::true(chdir('..'));
    Assert::true(rmdir('nested'));
} finally {
    if (is_resource($stream)) {
        fclose($stream);
    }
    chdir($originalDirectory);
    foreach ([$directory . '/nested/' . $filename, $directory . '/nested/child/renamed'] as $path) {
        if (file_exists($path)) {
            unlink($path);
        }
    }
    foreach ([$directory . '/nested/child', $directory . '/nested', $directory] as $path) {
        if (is_dir($path)) {
            rmdir($path);
        }
    }
}
echo "Done\n";
?>
--EXPECT--
Done
