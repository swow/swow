--TEST--
swow_fs: rename
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!is_writable('/tmp'), '/tmp is not writable');
skip_if(!is_writable(__DIR__), 'current dir is not writable');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

@unlink('/tmp/swow-test-rename');
file_put_contents('/tmp/swow-test-rename', 'foo');
Assert::true(rename('/tmp/swow-test-rename', __DIR__ . '/swow-test-rename'));

echo 'Done' . PHP_EOL;
?>
--CLEAN--
<?php
@unlink('/tmp/swow-test-rename');
@unlink(__DIR__ . '/swow-test-rename');
?>
--EXPECT--
Done
