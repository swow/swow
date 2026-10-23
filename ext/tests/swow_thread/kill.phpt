--TEST--
swow_thread: kill
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
    while (true) {
        usleep(1000);
    }
});
$thread->start();
usleep(10000);

$thread->kill();

try {
    $thread->wait(1000);
    echo "not killed\n";
} catch (Swow\ThreadException $e) {
    echo "killed\n";
}

try {
    $thread->wait();
} catch (Swow\ThreadException $e) {
    echo "killed again\n";
}

var_dump($thread->isRunning());
var_dump($thread->getExitStatus());
?>
--EXPECT--
killed
killed again
bool(false)
int(-1)
