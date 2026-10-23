--TEST--
swow_thannel: close wakes blocked producers and consumers
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

$thannel = new Swow\Thannel(0);
$producer = new Swow\Thread(static function (Swow\Thannel $thannel): string {
    try {
        $thannel->push('x');
        return 'pushed';
    } catch (Swow\ThannelException $e) {
        return 'push closed';
    }
}, $thannel);
$producer->start();
usleep(50000);
$thannel->close();
var_dump($producer->wait());

$thannel = new Swow\Thannel(1);
$consumer = new Swow\Thread(static function (Swow\Thannel $thannel): string {
    try {
        $thannel->pop(1000);
        return 'popped';
    } catch (Swow\ThannelException $e) {
        return 'pop closed';
    }
}, $thannel);
$consumer->start();
usleep(50000);
$thannel->close();
var_dump($consumer->wait());
?>
--EXPECT--
string(11) "push closed"
string(10) "pop closed"
