--TEST--
swow_defer: caught exceptions stay local and uncaught exceptions retain the outer chain
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use function Swow\defer;

function unwindRequest(Closure $callback, Throwable $original): void
{
    defer($callback);
    throw $original;
}

$original = new RuntimeException('request');
try {
    unwindRequest(static function (): void {
        try {
            throw new LogicException('caught callback');
        } catch (LogicException $inner) {
            echo "inner previous\n";
            var_dump($inner->getPrevious());
        }
    }, $original);
} catch (Throwable $caught) {
    echo "outer identity\n";
    var_dump($caught === $original);
}

$callbackPrevious = new DomainException('callback previous');
$callbackException = new LogicException('callback', 0, $callbackPrevious);
$requestPrevious = new UnexpectedValueException('request previous');
$original = new RuntimeException('request', 0, $requestPrevious);
try {
    unwindRequest(static function () use ($callbackException): void {
        throw $callbackException;
    }, $original);
} catch (Throwable $caught) {
    echo "uncaught chain\n";
    var_dump(
        $caught === $callbackException,
        $caught->getPrevious() === $callbackPrevious,
        $callbackPrevious->getPrevious() === $original,
        $original->getPrevious() === $requestPrevious,
        $requestPrevious->getPrevious(),
    );
}

try {
    unwindRequest(static function () use ($original): void {
        throw $original;
    }, $original);
} catch (Throwable $caught) {
    echo "same exception\n";
    var_dump($caught === $original, $caught->getPrevious() === $requestPrevious);
}

function normalReturn(): void
{
    defer(static function (): void {
        throw new LogicException('normal return callback');
    });
}
try {
    normalReturn();
} catch (LogicException $caught) {
    echo $caught->getMessage(), "\n";
    var_dump($caught->getPrevious());
}
echo "Done\n";
?>
--EXPECT--
inner previous
NULL
outer identity
bool(true)
uncaught chain
bool(true)
bool(true)
bool(true)
bool(true)
NULL
same exception
bool(true)
bool(true)
normal return callback
NULL
Done
