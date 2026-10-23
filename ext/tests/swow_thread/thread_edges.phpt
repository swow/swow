--TEST--
swow_thread: start, wait and detach state errors
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

$thread = new Swow\Thread('strrev', 'abc');

try {
    $thread->wait();
} catch (Swow\ThreadException $e) {
    echo 'wait: ', $e->getMessage(), "\n";
}

try {
    $thread->detach();
} catch (Swow\ThreadException $e) {
    echo 'detach: ', $e->getMessage(), "\n";
}

$thread->start();

try {
    $thread->start();
} catch (Swow\ThreadException $e) {
    echo 'start: ', $e->getMessage(), "\n";
}

var_dump($thread->wait());

try {
    $thread->detach();
} catch (Swow\ThreadException $e) {
    echo 'detach: ', $e->getMessage(), "\n";
}

$thread = new Swow\Thread('strrev', 'abc');
$thread->start();
$thread->detach();
var_dump($thread->detach() === $thread);
usleep(100000);
?>
--EXPECT--
wait: Thread has not been started
detach: Thread has not been started
start: Thread has already been started
string(3) "cba"
detach: Thread has already been waited
bool(true)
