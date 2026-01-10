--TEST--
swow_closure: serialize function of class anonymous
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.closure_serializer=1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$o = new class(6) {
    private static int $staticA;

    public function __construct(
        private int $instanceA,
    ) {
    }

    public static function staticMethod(int $b): int
    {
        return self::$staticA * $b;
    }

    public function instanceMethod(int $b): int
    {
        return $this->instanceA * $b;
    }

    public function __serialize(): array
    {
        return ['instanceA' => $this->instanceA];
    }

    public function __unserialize(array $data): void
    {
        $this->instanceA = $data['instanceA'];
    }
};

$c = Closure::fromCallable([$o, 'instanceMethod']);
Assert::throws(static function () use ($c): void {
    serialize($c);
}, Error::class, expectMessage: 'Closure which is not user-defined anonymous function and has no name cannot be serialized');

$c = Closure::fromCallable([$o, 'staticMethod']);
Assert::throws(static function () use ($c): void {
    serialize($c);
}, Error::class, expectMessage: 'Closure which is not user-defined anonymous function and has no name cannot be serialized');

echo "Done\n";
?>
--EXPECT--
Done
