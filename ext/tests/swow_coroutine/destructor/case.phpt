--TEST--
swow_coroutine: destructor case
--SKIPIF--
<?php
require __DIR__ . '/../../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../../include/bootstrap.php';

use Swow\Coroutine;
use Swow\Sync\WaitReference;

$wr = new WaitReference();
$o1 = new class {
    public function __destruct()
    {
        echo 'Destruct in main' . PHP_LF;
        usleep(1000);
        echo 'Everything is done' . PHP_LF;
        Assert::same(Coroutine::getCurrent()->getId(), 0); // TODO: use constant
    }
};
$o2 = new class {
    public function __destruct()
    {
        echo 'Destructor in sub' . PHP_LF;
        usleep(1000);
        echo 'After WaitReference but i can still work' . PHP_LF;
        Assert::same(Coroutine::getCurrent()->getId(), 1); // TODO: use constant
    }
};

Coroutine::run(function () use ($wr, $o2) {
    $sleeper = new class(Coroutine::getCurrent()->getPrevious()) {
        public $coroutine;

        public function __construct(Coroutine $coroutine)
        {
            $this->coroutine = $coroutine;
        }

        public function __destruct()
        {
            echo 'Sleeping...' . PHP_LF;
            usleep(1000);
            echo 'Sleep done' . PHP_LF;
        }
    };
    while (!$sleeper) {
        continue;
    }
});

unset($o2);
WaitReference::wait($wr);

echo 'Exit now' . PHP_LF;

?>
--EXPECT--
Sleeping...
Sleep done
Exit now
Destructor in sub
After WaitReference but i can still work
Destruct in main
Everything is done
