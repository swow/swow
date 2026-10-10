--TEST--
swow_closure: executed static local state survives serialization
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.closure_serializer=1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

/** Restored documentation belongs to the generated closure. */
$counter = static function (): int {
    static $value = 4;
    return ++$value;
};
$initial = unserialize(serialize($counter));
Assert::same($initial(), 5);
Assert::same($counter(), 5);
$restored = unserialize(serialize($counter));
Assert::same($restored(), 6);
Assert::same($restored(), 7);
Assert::same($counter(), 6);
Assert::same((new ReflectionFunction($restored))->getDocComment(), (new ReflectionFunction($counter))->getDocComment());

$values = static function (string $value): array {
    static $history = [];
    static $prefix = 'value:';
    $history[] = $prefix . $value;
    return $history;
};
Assert::same($values('first'), ['value:first']);
$restoredValues = unserialize(serialize($values));
Assert::same($restoredValues('second'), ['value:first', 'value:second']);
Assert::same($values('original'), ['value:first', 'value:original']);

// Captured references keep the existing external binding policy.
$externalValue = 10;
$capture = static function () use (&$externalValue): int {
    return $externalValue;
};
$restoredCapture = unserialize(serialize($capture));
$externalValue = 11;
Assert::same($capture(), 11);
Assert::same($restoredCapture(), 11);
echo "Done\n";
?>
--EXPECT--
Done
