--TEST--
swow_closure: T_CURLY_OPEN
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$greeting = 'Hello';
$sayHello = serialize(static function (string $name) use ($greeting): void {
    echo "{$greeting} {$name}!\n";
});
$sayHello = unserialize($sayHello);
$sayHello(Swow::class);

echo "Done\n";
?>
--EXPECT--
Hello Swow!
Done
