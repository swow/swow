--TEST--
swow_thread: repeated detached and killed short-lived threads
--SKIPIF--
<?php
if (!PHP_ZTS) {
    die('skip ZTS only');
}
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.closure_serializer=1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

for ($i = 0; $i < 100; $i++) {
    $detached = new Swow\Thread(static function (): void {
    });
    $detached->start();
    $detached->detach();
    unset($detached);

    $killed = new Swow\Thread(static function (): void {
        usleep(0);
    });
    $killed->start();
    try {
        $killed->kill();
    } catch (Throwable $e) {
    }
    try {
        $killed->wait();
    } catch (Throwable $e) {
    }
}

echo "done\n";
?>
--EXPECT--
done
