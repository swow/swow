--TEST--
swow_fs: symbolic link metadata, broken links and target ownership
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
$target = tempnam(sys_get_temp_dir(), 'swow-link-probe-');
$link = $target . '-link';
$supported = @symlink($target, $link);
if ($supported) {
    unlink($link);
}
unlink($target);
skip_if(!$supported, 'temporary directory does not support creating symbolic links');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$directory = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'swow-links-' . getRandomBytes(12);
$target = $directory . DIRECTORY_SEPARATOR . 'target';
$renamed = $directory . DIRECTORY_SEPARATOR . 'renamed';
$link = $directory . DIRECTORY_SEPARATOR . 'link';
try {
    Assert::true(mkdir($directory));
    Assert::same(file_put_contents($target, 'original'), 8);
    Assert::true(symlink($target, $link));
    Assert::true(is_link($link));
    Assert::same(filetype($link), 'link');
    Assert::same(readlink($link), $target);
    Assert::same(realpath($link), realpath($target));
    Assert::same(stat($link)['size'], 8);
    Assert::same(file_get_contents($link), 'original');
    Assert::true(rename($target, $renamed));
    clearstatcache(true, $link);
    Assert::true(is_link($link));
    Assert::false(file_exists($link));
    Assert::false(realpath($link));
    Assert::true(is_array(lstat($link)));
    // Removing the broken alias must leave the moved target intact.
    Assert::true(unlink($link));
    Assert::same(file_get_contents($renamed), 'original');
    Assert::true(unlink($renamed));
} finally {
    foreach ([$link, $target, $renamed] as $path) {
        if (is_link($path) || file_exists($path)) {
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
