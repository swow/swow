--TEST--
swow_fs: relative symbolic link targets survive replacement and report broken targets
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!is_writable(getcwd()), 'startup working directory is not writable');
$target = tempnam(getcwd(), 'swow-relative-link-probe-');
$link = $target . '-link';
$supported = @symlink(basename($target), $link);
if ($supported) {
    unlink($link);
}
unlink($target);
skip_if(!$supported, 'startup working directory does not support creating relative symbolic links');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

// Windows PHP checks relative targets against the process directory, even with virtual CWD.
// Keep the link in the unchanged startup directory so creation and resolution share that base.
$name = 'swow-relative-links-' . getRandomBytes(12);
$directory = getcwd() . DIRECTORY_SEPARATOR . $name;
$target = $directory . DIRECTORY_SEPARATOR . 'target';
$renamed = $directory . DIRECTORY_SEPARATOR . 'renamed';
$link = $directory . '-link';
try {
    Assert::true(mkdir($directory));
    Assert::same(file_put_contents($target, 'original'), 8);
    Assert::true(symlink($name . DIRECTORY_SEPARATOR . 'target', $link));
    Assert::true(is_link($link));
    Assert::true(file_exists($link));
    Assert::same(file_get_contents($link), 'original');
    Assert::true(rename($target, $renamed));
    clearstatcache(true, $link);
    Assert::true(is_link($link));
    Assert::false(file_exists($link));
    Assert::false(realpath($link));
    Assert::same(file_put_contents($target, 'replacement'), 11);
    clearstatcache(true, $link);
    Assert::true(file_exists($link));
    Assert::true(is_string(realpath($link)));
    Assert::same(file_get_contents($link), 'replacement');
    Assert::true(unlink($target));
    Assert::true(unlink($link));
    Assert::same(file_get_contents($renamed), 'original');
} finally {
    if (is_link($link)) {
        unlink($link);
    }
    foreach ([$target, $renamed] as $path) {
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
