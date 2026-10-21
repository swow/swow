--TEST--
swow_defer: nested callbacks and coroutine switching isolate pending exceptions
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

use function Swow\defer;

function nestedCleanup(): void
{
    defer(static function (): void {
        try {
            throw new LogicException('nested');
        } catch (LogicException $caught) {
            echo "nested previous\n";
            var_dump($caught->getPrevious());
        }
    });
}

function failingRequest(): void
{
    defer(static function (): void {
        echo "first registered\n";
        nestedCleanup();
    });
    defer(static function (): void {
        echo "last registered\n";
        Coroutine::yield();
        echo "resumed\n";
        nestedCleanup();
        throw new LogicException('callback');
    });
    throw new RuntimeException('request');
}

$coroutine = Coroutine::run(static function (): void {
    try {
        failingRequest();
    } catch (LogicException $caught) {
        echo $caught->getMessage(), '/', $caught->getPrevious()?->getMessage(), "\n";
        var_dump($caught->getPrevious()?->getPrevious());
    }
});

$peer = Coroutine::run(static function (): void {
    try {
        throw new DomainException('peer');
    } catch (DomainException $caught) {
        echo $caught->getMessage(), " previous\n";
        var_dump($caught->getPrevious());
    }
});
$coroutine->resume();

$cancelled = Coroutine::run(static function (): void {
    defer(static function (): void {
        nestedCleanup();
        echo "cancelled cleanup\n";
    });
    Coroutine::yield();
    echo "Unexpected resume\n";
});
$cancelled->kill();
echo "Done\n";
?>
--EXPECT--
last registered
peer previous
NULL
resumed
nested previous
NULL
first registered
nested previous
NULL
callback/request
NULL
nested previous
NULL
cancelled cleanup
Done
