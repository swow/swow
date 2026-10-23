--TEST--
swow_thread: getCurrent in a child thread
--SKIPIF--
<?php
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

$thread = new Swow\Thread(static function (): array {
    $current = Swow\Thread::getCurrent();

    return [
        'same' => $current === Swow\Thread::getCurrent(),
        'class' => $current instanceof Swow\Thread,
        'started' => $current->isStarted(),
        'running' => $current->isRunning(),
        'id' => $current->getId(),
        'id_match' => $current->getId() === Swow\Thread::getCurrentId(),
    ];
});
$thread->start();
$result = $thread->wait();

var_dump($result['same']);
var_dump($result['class']);
var_dump($result['started']);
var_dump($result['running']);
var_dump($result['id_match']);
var_dump($result['id'] === $thread->getId());
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
