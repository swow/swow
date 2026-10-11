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
$relativeLink = $directory . DIRECTORY_SEPARATOR . 'relative-link';
$chainLink = $directory . DIRECTORY_SEPARATOR . 'chain-link';
$targetDirectory = $directory . DIRECTORY_SEPARATOR . 'target-directory';
$movedDirectory = $directory . DIRECTORY_SEPARATOR . 'moved-directory';
$directoryLink = $directory . DIRECTORY_SEPARATOR . 'directory-link';
$workingDirectory = getcwd();
try {
    Assert::true(mkdir($directory));
    Assert::true(chdir($directory));
    Assert::same(file_put_contents($target, 'original'), 8);
    Assert::true(symlink($target, $link));
    Assert::true(is_link($link));
    Assert::same(filetype($link), 'link');
    // Windows may return a short path for the same target; compare the resolved file identity.
    $targetStat = stat($target);
    foreach ([readlink($link), realpath($link)] as $resolvedTarget) {
        Assert::true(is_string($resolvedTarget));
        $resolvedStat = stat($resolvedTarget);
        Assert::same($resolvedStat['dev'], $targetStat['dev']);
        Assert::same($resolvedStat['ino'], $targetStat['ino']);
    }
    Assert::same(stat($link)['size'], 8);
    Assert::same(file_get_contents($link), 'original');
    Assert::true(symlink('target', $relativeLink));
    Assert::true(file_exists($relativeLink));
    Assert::same(file_get_contents($relativeLink), 'original');
    Assert::true(symlink($link, $chainLink));
    Assert::true(file_exists($chainLink));
    Assert::same(file_get_contents($chainLink), 'original');
    Assert::true(rename($target, $renamed));
    clearstatcache(true, $link);
    Assert::true(is_link($link));
    Assert::false(file_exists($link));
    Assert::false(realpath($link));
    Assert::true(is_array(lstat($link)));
    foreach ([$relativeLink, $chainLink] as $alias) {
        clearstatcache(true, $alias);
        Assert::true(is_link($alias));
        Assert::false(file_exists($alias));
        Assert::false(realpath($alias));
    }
    Assert::false(file_exists('file://' . $link));

    // Restoring the target makes both aliases usable again without replacing the links.
    Assert::same(file_put_contents($target, 'replacement'), 11);
    foreach ([$link, $relativeLink, $chainLink] as $alias) {
        clearstatcache(true, $alias);
        Assert::true(file_exists($alias));
        Assert::true(is_string(realpath($alias)));
        Assert::same(file_get_contents($alias), 'replacement');
    }
    Assert::true(unlink($target));
    Assert::true(unlink($chainLink));
    Assert::true(unlink($relativeLink));
    // Removing the broken alias must leave the moved target intact.
    Assert::true(unlink($link));
    Assert::same(file_get_contents($renamed), 'original');
    Assert::true(unlink($renamed));

    Assert::true(mkdir($targetDirectory));
    Assert::true(symlink($targetDirectory, $directoryLink));
    Assert::true(file_exists($directoryLink));
    Assert::true(is_dir($directoryLink));
    Assert::true(is_string(realpath($directoryLink)));
    Assert::true(rename($targetDirectory, $movedDirectory));
    clearstatcache(true, $directoryLink);
    Assert::true(is_link($directoryLink));
    Assert::false(file_exists($directoryLink));
    Assert::false(realpath($directoryLink));
    Assert::true(unlink($directoryLink));
    Assert::true(is_dir($movedDirectory));
} finally {
    chdir($workingDirectory);
    foreach ([$directoryLink, $chainLink, $relativeLink, $link, $target, $renamed] as $path) {
        if (is_link($path) || file_exists($path)) {
            unlink($path);
        }
    }
    foreach ([$targetDirectory, $movedDirectory] as $path) {
        if (is_dir($path)) {
            rmdir($path);
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
