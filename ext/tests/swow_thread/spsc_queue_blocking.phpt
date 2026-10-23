--TEST--
swow_thread: SpscQueue blocking
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

use Swow\SpscQueue;
use Swow\SpscQueueException;
use Swow\Thread;

function waitFile(string $file): string
{
    $deadline = microtime(true) + 5;
    do {
        clearstatcache(true, $file);
        if (is_file($file)) {
            $content = file_get_contents($file);
            if ($content !== false && $content !== '') {
                return $content;
            }
        }
        usleep(10000);
    } while (microtime(true) < $deadline);

    throw new RuntimeException("Timed out waiting for {$file}");
}

$file = tempnam(sys_get_temp_dir(), 'swow-spsc-');
Assert::true(is_string($file));
@unlink($file);

try {
    $queue = new SpscQueue(1, 64);
    $thread = Thread::run(static function () use ($queue, $file): void {
        try {
            file_put_contents($file, serialize($queue->pop()));
        } catch (Throwable $e) {
            file_put_contents($file, $e::class);
        }
    });

    usleep(100000);
    $queue->push('blocked pop wake');
    $thread->wait();
    Assert::same('blocked pop wake', unserialize(waitFile($file)));
    Assert::true($queue->getParkCount() >= 1);
    Assert::true($queue->getWakeCount() >= 1);
} finally {
    @unlink($file);
}

$file = tempnam(sys_get_temp_dir(), 'swow-spsc-');
Assert::true(is_string($file));
@unlink($file);

try {
    $queue = new SpscQueue(1, 64);
    $queue->push('full');

    $thread = Thread::run(static function () use ($queue, $file): void {
        try {
            $queue->push('space wake');
            file_put_contents($file, 'pushed');
        } catch (Throwable $e) {
            file_put_contents($file, $e::class);
        }
    });

    usleep(100000);
    Assert::same('full', $queue->pop());
    $thread->wait();
    Assert::same('pushed', waitFile($file));
} finally {
    @unlink($file);
}

$file = tempnam(sys_get_temp_dir(), 'swow-spsc-');
Assert::true(is_string($file));
@unlink($file);

try {
    $queue = new SpscQueue(1, 64);
    $thread = Thread::run(static function () use ($queue, $file): void {
        try {
            $queue->pop();
            file_put_contents($file, 'unexpected');
        } catch (SpscQueueException) {
            file_put_contents($file, 'closed');
        }
    });

    usleep(100000);
    $queue->close();
    $thread->wait();
    Assert::same('closed', waitFile($file));
} finally {
    @unlink($file);
}

echo "Done\n";

?>
--EXPECT--
Done
