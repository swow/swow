--TEST--
swow_closure: unserialize reports the exception thrown by the restored code
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--INI--
swow.closure_serializer=1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

// The compiled code never assigns the return value when it throws, so the
// restore path must not inspect the unwritten value and report "got null"
// instead of the original exception.
$closure = static fn(): int => 0;
try {
    $closure->__unserialize(['code' => 'throw new Exception("boom");']);
    Assert::fail('the code threw, so __unserialize must not return normally');
} catch (Exception $exception) {
    Assert::same($exception->getMessage(), 'boom');
} catch (Throwable $exception) {
    Assert::fail('expected the original Exception, got ' . get_class($exception) . ': ' . $exception->getMessage());
}
Assert::same($closure(), 0);

// An undefined function is an Error, not an Exception.
try {
    $closure->__unserialize(['code' => 'undefined_function_call();']);
    Assert::fail('the code threw, so __unserialize must not return normally');
} catch (Error $error) {
    Assert::contains($error->getMessage(), 'undefined_function_call');
} catch (Throwable $exception) {
    Assert::fail('expected the original Error, got ' . get_class($exception) . ': ' . $exception->getMessage());
}
Assert::same($closure(), 0);

// A regular round trip still works after those failures.
$restored = unserialize(serialize(static fn(): int => 42));
Assert::same($restored(), 42);

echo "Done\n";

?>
--EXPECT--
Done
