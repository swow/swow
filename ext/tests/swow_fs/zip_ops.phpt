--TEST--
swow_fs: zip fs operations
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_extension_not_exist('zlib');
skip_if_extension_not_exist('zip');
?>
--FILE--
<?php

set_error_handler(function ($errno, $errstr, $errfile, $errline) {
    var_dump($errno, $errstr, $errfile, $errline);
    exit(1);
});

define('testzip', __DIR__ . DIRECTORY_SEPARATOR . 'zip_ops.zip');
define('testnotzip', __DIR__ . DIRECTORY_SEPARATOR . 'zip_ops.notzip');

if (file_exists(testzip)) {
    unlink(testzip);
}
$zip = new ZipArchive();
$zip->open(testzip, ZipArchive::CREATE);
$zip->addFromString('file_1', "this is content of file_1.\n");
$zip->addFromString('file_2', "this is content of file_2.\n");
$zip->addFromString('file_3', "this is content of file_3.\n");
$zip->addEmptyDir('dir_a');
$zip->addEmptyDir('dir_b');
$zip->addEmptyDir('dir_c');
$zip->addFromString('dir_a/file_a2', "this is content of file_a2.\n");
$zip->addFromString('dir_c/file_c3', "this is content of file_c3.\n");

$zip->close();

rename(testzip, testnotzip);

$zip = new ZipArchive();
$zip->open(testnotzip);

for ($i = 0; $i < $zip->count(); $i++) {
    $name = $zip->getNameIndex($i);
    if (substr($name, -1) !== '/') {
        $fns = explode('/', $name);
        $fn = array_reverse($fns)[0];
        $content = file_get_contents('zip://' . testnotzip . '#' . $name);
        $expected = "this is content of {$fn}.\n";
        if ($content !== $expected) {
            throw new Exception("bad file context for {$name}: got '{$content}', excepted '{$expected}'");
        }
    }
}

echo 'Done' . PHP_EOL;
?>
--CLEAN--
<?php
@unlink(__DIR__ . DIRECTORY_SEPARATOR . 'zip_ops.zip');
@unlink(__DIR__ . DIRECTORY_SEPARATOR . 'zip_ops.notzip');
?>
--EXPECT--
Done
