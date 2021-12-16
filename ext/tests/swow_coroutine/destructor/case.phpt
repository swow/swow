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
        Assert::greaterThan(Coroutine::getCurrent()->getId(), 0);
    }
};
$o2 = new class {
    public function __destruct()
    {
        echo 'Destructor in sub' . PHP_LF;
        usleep(1000);
        echo 'Never here because coroutine will be killed in shutdown dtor' . PHP_LF;
    }
}; // Notice: o2 will be bound to coroutine#2

Coroutine::run(function () use ($wr, $o2) {
    /** @noinspection PhpUnusedLocalVariableInspection */
    $sleeper = new class(Coroutine::getCurrent()->getPrevious()) {
        public Coroutine $coroutine;

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
});

unset($o2);
WaitReference::wait($wr);

echo 'Exit now' . PHP_LF;

?>
--EXPECT--
Sleeping...
Sleep done
Exit now
Destruct in main
Destructor in sub
Everything is done
