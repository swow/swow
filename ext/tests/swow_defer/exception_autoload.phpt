--TEST--
swow_defer: autoload during exception unwinding
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use function Swow\defer;

spl_autoload_register(static function (string $name): void {
    if (!in_array($name, ['DeferredPreloaded', 'DeferredAutoloaded'], true)) {
        return;
    }
    eval('class ' . $name . ' {
        public static function run(): void {
            $headers = ["header"];
            if ($headers === []) {
                throw new LogicException("Unexpected empty headers");
            }
            echo "callback completed\n";
        }
    }');
});

function failingRequest(string $class, RuntimeException $original): void
{
    defer(static function () use ($class): void {
        $class::run();
        echo "defer completed\n";
    });
    throw $original;
}

class_exists('DeferredPreloaded');
foreach (['DeferredPreloaded', 'DeferredAutoloaded'] as $class) {
    echo $class, "\n";
    $original = new RuntimeException('request');
    try {
        failingRequest($class, $original);
    } catch (Throwable $caught) {
        var_dump($caught === $original, $caught->getPrevious());
    }
}
echo "Done\n";
?>
--EXPECT--
DeferredPreloaded
callback completed
defer completed
bool(true)
NULL
DeferredAutoloaded
callback completed
defer completed
bool(true)
NULL
Done
