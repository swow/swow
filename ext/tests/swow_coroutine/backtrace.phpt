--TEST--
swow_coroutine: backtrace
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

class Foo
{
    public function __debugInfo()
    {
        for ($level = -3; $level < 3; $level++) {
            for ($limit = $level < 0 ? -$level : ($level > 0 ? 3 - $level : 3) ; $limit >= -1; $limit--) {
                echo sprintf('level=%d, limit=%d', $level, $limit) . PHP_LF;
                echo Coroutine::getCurrent()->getTraceAsString($level, $limit) . PHP_LF;
                echo PHP_LF;
            }
        }

        return ['Hello' => 'World'];
    }
}

var_dump(new Foo);

echo 'Done' . PHP_LF;

?>
--EXPECTF--
level=-3, limit=3
#0 %sbacktrace.php(%d): Swow\Coroutine->getTraceAsString(-3, 3)
#1 [internal function]: Foo->__debugInfo()
#2 %sbacktrace.php(%d): var_dump(Object(Foo))
#3 {main}

level=-3, limit=2
#0 %sbacktrace.php(%d): Swow\Coroutine->getTraceAsString(-3, 2)
#1 [internal function]: Foo->__debugInfo()
#2 {main}

level=-3, limit=1
#0 %sbacktrace.php(%d): Swow\Coroutine->getTraceAsString(-3, 1)
#1 {main}

level=-3, limit=0
#0 %sbacktrace.php(%d): Swow\Coroutine->getTraceAsString(-3, 0)
#1 [internal function]: Foo->__debugInfo()
#2 %sbacktrace.php(%d): var_dump(Object(Foo))
#3 {main}

level=-3, limit=-1
#0 %sbacktrace.php(%d): Swow\Coroutine->getTraceAsString(-3, -1)
#1 [internal function]: Foo->__debugInfo()
#2 %sbacktrace.php(%d): var_dump(Object(Foo))
#3 {main}

level=-2, limit=2
#0 [internal function]: Foo->__debugInfo()
#1 %sbacktrace.php(%d): var_dump(Object(Foo))
#2 {main}

level=-2, limit=1
#0 [internal function]: Foo->__debugInfo()
#1 {main}

level=-2, limit=0
#0 [internal function]: Foo->__debugInfo()
#1 %sbacktrace.php(%d): var_dump(Object(Foo))
#2 {main}

level=-2, limit=-1
#0 [internal function]: Foo->__debugInfo()
#1 %sbacktrace.php(%d): var_dump(Object(Foo))
#2 {main}

level=-1, limit=1
#0 %sbacktrace.php(%d): var_dump(Object(Foo))
#1 {main}

level=-1, limit=0
#0 %sbacktrace.php(%d): var_dump(Object(Foo))
#1 {main}

level=-1, limit=-1
#0 %sbacktrace.php(%d): var_dump(Object(Foo))
#1 {main}

level=0, limit=3
#0 %sbacktrace.php(%d): Swow\Coroutine->getTraceAsString(0, 3)
#1 [internal function]: Foo->__debugInfo()
#2 %sbacktrace.php(%d): var_dump(Object(Foo))
#3 {main}

level=0, limit=2
#0 %sbacktrace.php(%d): Swow\Coroutine->getTraceAsString(0, 2)
#1 [internal function]: Foo->__debugInfo()
#2 {main}

level=0, limit=1
#0 %sbacktrace.php(%d): Swow\Coroutine->getTraceAsString(0, 1)
#1 {main}

level=0, limit=0
#0 %sbacktrace.php(%d): Swow\Coroutine->getTraceAsString(0, 0)
#1 [internal function]: Foo->__debugInfo()
#2 %sbacktrace.php(%d): var_dump(Object(Foo))
#3 {main}

level=0, limit=-1
#0 %sbacktrace.php(%d): Swow\Coroutine->getTraceAsString(0, -1)
#1 [internal function]: Foo->__debugInfo()
#2 %sbacktrace.php(%d): var_dump(Object(Foo))
#3 {main}

level=1, limit=2
#0 [internal function]: Foo->__debugInfo()
#1 %sbacktrace.php(%d): var_dump(Object(Foo))
#2 {main}

level=1, limit=1
#0 [internal function]: Foo->__debugInfo()
#1 {main}

level=1, limit=0
#0 [internal function]: Foo->__debugInfo()
#1 %sbacktrace.php(%d): var_dump(Object(Foo))
#2 {main}

level=1, limit=-1
#0 [internal function]: Foo->__debugInfo()
#1 %sbacktrace.php(%d): var_dump(Object(Foo))
#2 {main}

level=2, limit=1
#0 %sbacktrace.php(%d): var_dump(Object(Foo))
#1 {main}

level=2, limit=0
#0 %sbacktrace.php(%d): var_dump(Object(Foo))
#1 {main}

level=2, limit=-1
#0 %sbacktrace.php(%d): var_dump(Object(Foo))
#1 {main}

object(Foo)#%d (1) {
  ["Hello"]=>
  string(5) "World"
}
Done
