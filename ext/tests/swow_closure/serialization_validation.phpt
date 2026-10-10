--TEST--
swow_closure: serialized data validation and named function round trips
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.closure_serializer=1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$invalidData = [
    [],
    ['code' => null],
    ['code' => 42],
    ['code' => 'return static fn(): int => 1;', 'file' => 42],
    ['code' => 'return static fn(): int => 1;', 'doc_comment' => []],
    ['code' => 'return static fn(): int => 1;', 'static_variables' => 'invalid'],
    ['code' => 'return static fn(): int => 1;', 'static_variables' => [1]],
];
foreach ($invalidData as $data) {
    $closure = static fn(): int => 0;
    Assert::throws(static function () use ($closure, $data): void {
        $closure->__unserialize($data);
    }, ValueError::class);
    Assert::same($closure(), 0);
}
foreach (['return null;', 'return 42;', 'return new stdClass();'] as $code) {
    $closure = static fn(): int => 0;
    Assert::throws(static function () use ($closure, $code): void {
        $closure->__unserialize(['code' => $code]);
    }, Error::class);
    Assert::same($closure(), 0);
}

function closureValidationLength(string $value): int
{
    return strlen($value);
}
foreach ([Closure::fromCallable('strlen'), Closure::fromCallable('closureValidationLength')] as $closure) {
    $restored = unserialize(serialize($closure));
    Assert::same($restored("a\0b"), 3);
}
$owner = new stdClass();
$bound = (function (): object {
    return $this;
})->bindTo($owner);
Assert::throws(static fn() => serialize($bound), Error::class);
Assert::same($bound(), $owner);
echo "Done\n";
?>
--EXPECT--
Done
