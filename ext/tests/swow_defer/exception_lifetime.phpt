--TEST--
swow_defer: repeated caught and chained exceptions release their objects
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use function Swow\defer;

function failingRequest(bool $catchInside, Throwable $original): void
{
    defer(static function () use ($catchInside): void {
        try {
            throw new LogicException('callback');
        } catch (LogicException $caught) {
            if (!$catchInside) {
                throw $caught;
            }
        }
    });
    throw $original;
}

$released = 0;
foreach ([true, false] as $catchInside) {
    for ($iteration = 0; $iteration < 64; $iteration++) {
        $original = new RuntimeException('request');
        $reference = WeakReference::create($original);
        try {
            failingRequest($catchInside, $original);
        } catch (Throwable $caught) {
            $activeReference = WeakReference::create($caught);
        }
        unset($original, $caught);
        if ($reference->get() === null && $activeReference->get() === null) {
            $released++;
        }
    }
}
echo 'Released: ', $released, "\n";
?>
--EXPECT--
Released: 128
