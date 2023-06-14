--TEST--
swow_coroutine: extends properties destruction
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

foreach ([false, true] as $throwError) {
    (new class(static function (): void {
        msleep(0);
    }, $throwError) extends Coroutine {
        public object $aaa;
        public bool $throwError;

        public function __construct(callable $callable, bool $throwError)
        {
            parent::__construct($callable);
            $this->aaa = new class() {
                public function __destruct()
                {
                    echo "Destruct\n";
                    var_dump(Coroutine::getCurrent()->aaa === $this);
                    $throwError = Coroutine::getCurrent()->throwError;
                    if ($throwError) {
                        throw new Error();
                    }
                }
            };
            $this->throwError = $throwError;
        }
    })->resume();
    msleep(1);
    echo "\n";
}

(new class(static function (): void {
    msleep(0);
}) extends Coroutine {
    public bool $bbb;
    public object $aaa;

    public function __construct(callable $callable)
    {
        parent::__construct($callable);
        $this->bbb = true;
        $this->aaa = new class() {
            public function __destruct()
            {
                echo "Destruct\n";
                var_dump(isset(Coroutine::getCurrent()->bbb));
            }
        };
    }
})->resume();
msleep(1);
echo "\n";

echo "Done\n";

?>
--EXPECTF--
Destruct
bool(true)

Destruct
bool(true)

Warning: [Fatal error in R%d] Uncaught Error in %sextends_properties_destruction.php:%d
Stack trace:
#0 [internal function]: class@anonymous->__destruct()
#1 {main}
  thrown in %sextends_properties_destruction.php on line %d

Destruct
bool(false)

Done
