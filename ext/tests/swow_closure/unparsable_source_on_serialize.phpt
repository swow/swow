--TEST--
swow_closure: serializing after the source file became unparsable does not crash
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.closure_serializer=1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$file = sys_get_temp_dir() . '/swow_closure_broken_' . getRandomBytes(8) . '.php';
register_shutdown_function(static function () use ($file): void {
    @unlink($file);
});

$good = <<<'PHP'
<?php

return function () {
    return 42;
};
PHP;
file_put_contents($file, $good);
$closure = require $file;
Assert::same($closure(), 42);

// The tokenizer reads the file again while the closure is serialized. An
// unparsable file must end the serialization with an exception.
file_put_contents($file, "<?php\nreturn function () {\n");

Assert::throws(static function () use ($closure): void {
    serialize($closure);
}, Throwable::class);

// The scanner and compiler state must still be usable afterwards.
Assert::same(strlen('still works'), 11);
$other = static fn(): int => 7;
Assert::same($other(), 7);
Assert::same(unserialize(serialize($other))(), 7);

echo "Done\n";

?>
--EXPECT--
Done
