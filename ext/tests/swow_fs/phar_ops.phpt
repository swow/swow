--TEST--
swow_fs: phar fs operations
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_extension_not_exist('phar');
$ext_enable = '';
$loaded1 = shell_exec(PHP_BINARY . ' -m');
if (strpos($loaded1, 'Swow') === false) {
    $ext_enable .= ' -dextension=swow';
}
if (strpos($loaded1, 'Phar') === false) {
    $ext_enable .= ' -dextension=phar';
}
$loaded2 = shell_exec(PHP_BINARY . $ext_enable . ' -m');
if (
    strpos($loaded2, 'Phar') === false ||
    strpos($loaded2, 'Swow') === false ||
    strpos($loaded2, 'Warning') !== false
) {
    skip('Swow or phar is not present in TEST_PHP_EXECUTABLE and cannot load it via -dextension');
}
?>
--INI--
phar.readonly=0
--FILE--
<?php

set_error_handler(function ($errno, $errstr, $errfile, $errline) {
    var_dump($errno, $errstr, $errfile, $errline);
    exit(1);
});

define('testphar', __DIR__ . DIRECTORY_SEPARATOR . 'phar_ops.phar');

const remote_lister = <<<'EOF'
<?php

set_error_handler(function ($errno, $errstr, $errfile, $errline) {
    var_dump($errno, $errstr, $errfile, $errline);
    exit(1);
});

// comprehensive local modifying
$f = fopen(__DIR__ . DIRECTORY_SEPARATOR . 'dir_b/file_new_c', 'w+');
fwrite($f, 'this is content ');
fwrite($f, "of file_new_c.\n");
fflush($f);
fclose($f);
$f = fopen(__DIR__ . DIRECTORY_SEPARATOR . 'dir_b/file_new_c', 'r');
$content = fread($f, 4096);
if ($content !== "this is content of file_new_c.\n") {
    throw new Exception("bad file context for {dir_b/file_new_c}: got '{$content}', excepted 'this is content of file_new_c.\n'");
}

// remote modifying
file_put_contents(__DIR__ . DIRECTORY_SEPARATOR . 'file_not_new', "this is content of file_not_new.\n");
unlink(__DIR__ . DIRECTORY_SEPARATOR . 'file_2');
file_put_contents(__DIR__ . DIRECTORY_SEPARATOR . 'dir_b/file_not_new_b', "this is content of file_not_new_b.\n");
rename(__DIR__ . DIRECTORY_SEPARATOR . 'dir_c/file_a2', __DIR__ . DIRECTORY_SEPARATOR . 'file_a2');
rmdir(__DIR__ . DIRECTORY_SEPARATOR . 'dir_d');
mkdir(__DIR__ . DIRECTORY_SEPARATOR . 'dir_a');

// remote list
$it = new RecursiveIteratorIterator(
    new RecursiveDirectoryIterator(__DIR__),
    RecursiveIteratorIterator::SELF_FIRST
);
foreach ($it as $f) {
    $fn = $f->getFileName();
    $path = $f->getPathName();
    $st = stat($path);
    if ($fn === 'remote.php') {
        // skip
    } elseif (!is_dir($path)) {
        $expected = "this is content of {$fn}.\n";
        if (($content = file_get_contents($path)) !== $expected) {
            throw new Exception("bad file context for {$path}: got '{$content}', excepted '{$expected}'");
        }
    }
}

echo 'Done remote' . PHP_EOL;
EOF;

if (file_exists(testphar)) {
    unlink(testphar);
}
$phar = new Phar(testphar);
$phar->addFromString('file_1', "this is content of file_1.\n");
$phar->addFromString('file_2', "this is content of file_2.\n");
$phar->addFromString('file_3', "this is content of file_3.\n");
$phar->addEmptyDir('dir_a');
$phar->addEmptyDir('dir_b');
$phar->addEmptyDir('dir_c');
$phar->addFromString('dir_a/file_a2', "this is content of file_a2.\n");
$phar->addFromString('dir_c/file_c3', "this is content of file_c3.\n");

$phar->addFromString('remote.php', remote_lister);

$phar->stopBuffering();
$phar->setStub($phar->createDefaultStub('remote.php'));

// local modifying
file_put_contents('phar://' . testphar . DIRECTORY_SEPARATOR . 'file_new', "this is content of file_new.\n");
unlink('phar://' . testphar . DIRECTORY_SEPARATOR . 'file_1');
file_put_contents('phar://' . testphar . DIRECTORY_SEPARATOR . 'dir_b/file_new_b', "this is content of file_new_b.\n");
rename('phar://' . testphar . DIRECTORY_SEPARATOR . 'dir_a/file_a2', 'phar://' . testphar . DIRECTORY_SEPARATOR . 'dir_c/file_a2');
rmdir('phar://' . testphar . DIRECTORY_SEPARATOR . 'dir_a');
mkdir('phar://' . testphar . DIRECTORY_SEPARATOR . 'dir_d');

// comprehensive local modifying
$f = fopen('phar://' . testphar . DIRECTORY_SEPARATOR . 'dir_b/file_new_c', 'w+');
fwrite($f, 'this is content ');
fwrite($f, "of file_new_c.\n");
fflush($f);
fclose($f);
$f = fopen('phar://' . testphar . DIRECTORY_SEPARATOR . 'dir_b/file_new_c', 'r');
$content = fread($f, 4096);
if ($content !== "this is content of file_new_c.\n") {
    throw new Exception("bad file context for {dir_b/file_new_c}: got '{$content}', excepted 'this is content of file_new_c.\n'");
}

// local list
$it = new RecursiveIteratorIterator(
    new RecursiveDirectoryIterator('phar://' . testphar),
    RecursiveIteratorIterator::SELF_FIRST
);
foreach ($it as $f) {
    $fn = $f->getFileName();
    $path = $f->getPathName();
    $st = stat($path);
    if ($fn === 'remote.php') {
        if (($content = file_get_contents($path)) !== remote_lister) {
            throw new Exception("bad file context for {$path}: got '{$content}', excepted '" . remote_lister . "'");
        }
    } elseif (!is_dir($path)) {
        $expected = "this is content of {$fn}.\n";
        if (($content = file_get_contents($path)) !== $expected) {
            throw new Exception("bad file context for {$path}: got '{$content}', excepted '{$expected}'");
        }
    }
}

// remote test
$ext_enable = ' -dphar.readonly=0';
$loaded = shell_exec(PHP_BINARY . ' -m');
if (strpos($loaded, 'Swow') === false) {
    $ext_enable .= ' -dextension=swow';
}
if (strpos($loaded, 'Phar') === false) {
    $ext_enable .= ' -dextension=phar';
}
passthru(PHP_BINARY . $ext_enable . ' ' . testphar);

echo 'Done' . PHP_EOL;
?>
--CLEAN--
<?php
@unlink(__DIR__ . DIRECTORY_SEPARATOR . 'phar_ops.phar');
?>
--EXPECT--
Done remote
Done
