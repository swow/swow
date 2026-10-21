--TEST--
swow_defer: fatal callback errors propagate during exception unwinding
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use function Swow\defer;

function failingRequest(): void
{
    defer(static function (): void {
        echo "defer entered\n";
        eval('function invalidReturn(): void { return 1; }');
        echo "Unexpected callback completion\n";
    });
    throw new RuntimeException('request');
}

try {
    failingRequest();
} catch (Throwable $caught) {
    echo "Unexpected catch\n";
}
echo "Unexpected completion\n";
?>
--EXPECTREGEX--
(?![\s\S]*Unexpected)defer entered\n[\s\S]*Fatal error: A void function must not return a value in [^\n]+ on line [0-9]+[\s\S]*
