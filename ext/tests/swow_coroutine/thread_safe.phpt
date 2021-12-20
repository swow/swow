--TEST--
swow_coroutine: thread safe
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
if (!PHP_ZTS || !extension_loaded('pthreads')) {
    exit('skip if nts or no pthreads');
}
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

class Corker extends Thread
{
    protected static int $num = 0;
    protected int $id;

    public function __construct()
    {
        $this->id = static::$num++;
    }

    public function run()
    {
        if ($this->id === 6) {
            usleep(10000);
        }
        $coroutine = Coroutine::run(function ($a) {
            $b = Coroutine::yield();
            return $a . ' ' . $b;
        }, 'hello');
        echo $coroutine->resume('world') . ' #' . $this->id . PHP_LF;
    }
}

$workers = [];
for ($i = 0; $i < 10; $i++) {
    $worker = new Corker;
    if ($worker->start()) {
        $workers[] = $worker;
    }
}
foreach ($workers as $worker) {
    $worker->join();
}

echo 'Done' . PHP_LF;

?>
--EXPECTF--
hello world #%d
hello world #%d
hello world #%d
hello world #%d
hello world #%d
hello world #%d
hello world #%d
hello world #%d
hello world #%d
hello world #6
Done
