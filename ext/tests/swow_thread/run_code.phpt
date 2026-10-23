--TEST--
swow_thread: run code
--SKIPIF--
<?php
if (!PHP_ZTS) {
    exit('skip ZTS only');
}
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$file = tempnam(sys_get_temp_dir(), 'swow-thread-code-');
unlink($file);

$thread = Swow\Thread::runCode(
    'file_put_contents(' . var_export($file, true) . ', "code");'
);
$thread->wait();

var_dump($thread->getExitStatus());
var_dump(file_get_contents($file));
unlink($file);
?>
--EXPECT--
int(0)
string(4) "code"
