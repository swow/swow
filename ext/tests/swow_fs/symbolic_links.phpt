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
$nativeProbe = $directory . DIRECTORY_SEPARATOR . 'native-probe.php';
try {
    Assert::true(mkdir($directory));
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
    Assert::true(rename($target, $renamed));
    clearstatcache(true, $link);
    Assert::true(is_link($link));
    if (file_exists($link)) {
        // An unhooked process distinguishes native filesystem behavior from Swow hooks.
        file_put_contents($nativeProbe, <<<'PHP'
<?php
echo json_encode([
    'swow_loaded' => extension_loaded('swow'),
    'zts' => PHP_ZTS,
    'file_exists' => file_exists($argv[1]),
    'is_link' => is_link($argv[1]),
    'realpath' => realpath($argv[1]),
], JSON_THROW_ON_ERROR), PHP_EOL;
PHP);
        echo 'Native broken-link probe: ', shell_exec(
            escapeshellarg(PHP_BINARY) . ' -n ' . escapeshellarg($nativeProbe) . ' ' . escapeshellarg($link)
        );
    }
    Assert::false(file_exists($link));
    Assert::false(realpath($link));
    Assert::true(is_array(lstat($link)));
    // Removing the broken alias must leave the moved target intact.
    Assert::true(unlink($link));
    Assert::same(file_get_contents($renamed), 'original');
    Assert::true(unlink($renamed));
} finally {
    foreach ([$nativeProbe, $link, $target, $renamed] as $path) {
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
