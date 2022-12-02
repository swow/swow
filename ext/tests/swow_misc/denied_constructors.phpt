--TEST--
swow_misc: create not creatable classes
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

function try_create(string $class): void
{
    try {
        $ret = new $class();
        echo "{$class} should not be created\n";
        var_dump($ret);
    } catch (Error $e) {
        Assert::same($e->getMessage(), "The object of {$class} can not be created for security reasons");
    }
}
foreach ([
    'Swow',
    'Swow\\Http\\Status',
    'Swow\\Log',
    'Swow\\Signal',
    'Swow\\Watchdog',
    'Swow\\WebSocket\\Opcode',
    'Swow\\WebSocket\\Status',
] as $class) {
    try_create($class);
}

echo "Done\n";
?>
--EXPECT--
Done
