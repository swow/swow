--TEST--
swow_fs: simple flock
--SKIPIF--
<?php
if (getenv('SWOW_TSAN') !== false) {
    exit('skip TSan cannot handle PHP bailout');
}
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
$file = sprintf('%s/testFlock.lock', sys_get_temp_dir());
var_dump($file);
if (is_file($file)) {
    unlink($file);
}
$h = fopen($file, 'w');
if (!$h) {
    exit('fopen failed');
}
echo "try lock with LOCK_EX | LOCK_NB\n";
var_dump(flock($h, LOCK_EX | LOCK_NB));
fclose($h);
unlink($file);
?>
--EXPECTF--
string(%d) "%AtestFlock.lock"
try lock with LOCK_EX | LOCK_NB
bool(true)
