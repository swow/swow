--TEST--
swow_fs: disabled file hooks retain native broken-link query results
--INI--
swow.async_file=0
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

Assert::same(ini_get('swow.async_file'), '0');
$target = tempnam(sys_get_temp_dir(), 'swow-sync-link-');
$link = $target . '-link';
try {
    Assert::true(symlink($target, $link));
    Assert::true(file_exists($link));
    Assert::true(is_string(realpath($link)));
    Assert::true(unlink($target));
    clearstatcache(true, $link);
    // The disabled hook must delegate even where native PHP has different broken-link results.
    $native = json_decode(shell_exec(
        escapeshellarg(PHP_BINARY) . ' -n -r ' . escapeshellarg(
            'echo json_encode([extension_loaded("swow"), file_exists($argv[1]), realpath($argv[1]), is_link($argv[1])], JSON_THROW_ON_ERROR);'
        ) . ' ' . escapeshellarg($link)
    ), true, flags: JSON_THROW_ON_ERROR);
    Assert::false($native[0]);
    Assert::same(file_exists($link), $native[1]);
    Assert::same(realpath($link), $native[2]);
    Assert::same(is_link($link), $native[3]);
    Assert::true(unlink($link));
} finally {
    if (is_link($link)) {
        unlink($link);
    }
    if (file_exists($target)) {
        unlink($target);
    }
}
echo "Done\n";
?>
--EXPECT--
Done
