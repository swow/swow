--TEST--
swow_thread: killAll
--SKIPIF--
<?php
if (getenv('SWOW_TSAN') !== false) {
    die('skip TSan cannot handle PHP bailout');
}
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

$threads = [];
for ($i = 0; $i < 2; $i++) {
    $thread = new Swow\Thread(static function (): void {
        while (true) {
            usleep(1000);
        }
    });
    $thread->start();
    $threads[] = $thread;
}

var_dump(Swow\Thread::killAll());

foreach ($threads as $thread) {
    try {
        $thread->wait(1000);
    } catch (Swow\ThreadException $e) {
        echo "killed\n";
    }
}

var_dump(Swow\Thread::killAll());
?>
--EXPECT--
int(2)
killed
killed
int(0)
