--TEST--
swow_thread: exit status
--SKIPIF--
<?php
if (getenv('SWOW_TSAN') !== false) {
    exit('skip TSan cannot handle PHP bailout');
}
if (!PHP_ZTS) {
    exit('skip ZTS only');
}
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.closure_serializer=1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$thread = new Swow\Thread(static function (): void {
    exit(7);
});
$thread->start();
var_dump($thread->wait());
var_dump($thread->getExitStatus());
?>
--EXPECT--
NULL
int(7)
