--TEST--
swow_thread: a thread that exits at once does not free the run of its parent
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

/*
 * The thread can exit before start() takes the reference that the object keeps.
 * The parent then waits on the semaphore of a run that the exiting thread has
 * already freed, which aborts the process on some platforms.
 */
for ($i = 0; $i < 300; $i++) {
    $thread = new Swow\Thread('intval', '42');
    $thread->start();
    $thread->wait();
}

echo "done\n";
?>
--EXPECT--
done
