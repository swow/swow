--TEST--
swow_closure: fn function
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

const A = 1;
$a = "this is a";
$fns =[
    // literal
    fn () => 1,
    fn () => "cafebabe",
    fn () => 3.14,
    fn () => true,
    fn () => false,
    fn () => null,
    fn () => [],
    fn () => [1, 2, 3],
    fn () => ["a" => 1, "b" => 2, "c" => 3],
    // create object
    fn () => new stdClass(),
    fn () => new ValueError(),
    // throw
    fn () => throw new AssertionError(),
    // reference
    fn () => $a,
    fn () => A,
    // expression
    fn () => 1 + 1,
    fn () => 1 - (1 + 2) * 3 >> 4,
    fn ($arg) => $arg + 1,
    // static
    // literal
    static fn () => 1,
    static fn () => "cafebabe",
    static fn () => 3.14,
    static fn () => true,
    static fn () => false,
    static fn () => null,
    static fn () => [],
    static fn () => [1, 2, 3],
    static fn () => ["a" => 1, "b" => 2, "c" => 3],
    // create object
    static fn () => new stdClass(),
    static fn () => new ValueError(),
    // throw
    static fn () => throw new AssertionError(),
    // reference
    static fn () => A,
    // expression
    static fn () => 1 + 1,
    static fn () => 1 - (1 + 2) * 3 >> 4,
    static fn ($arg) => $arg + 1,
];

foreach ($fns as $fn) {
    $serialized = serialize($fn);
    // var_dump($serialized);
    $unserialized = unserialize($serialized);
    try {
        $ret = $unserialized();
        if (is_a($ret, 'stdClass') || is_a($ret, 'ValueError')) {
            var_dump(get_class($ret));
        } else {
            var_dump($ret);
        }
    } catch (Exception $e) {
        var_dump($e);
    } catch (ArgumentCountError) {
        var_dump($unserialized(41));
    } catch (Error $e) {
        var_dump(get_class($e));
    }
}

echo "Done\n";
?>

--EXPECT--
int(1)
string(8) "cafebabe"
float(3.14)
bool(true)
bool(false)
NULL
array(0) {
}
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
array(3) {
  ["a"]=>
  int(1)
  ["b"]=>
  int(2)
  ["c"]=>
  int(3)
}
string(8) "stdClass"
string(10) "ValueError"
string(14) "AssertionError"
string(9) "this is a"
int(1)
int(2)
int(-1)
int(42)
int(1)
string(8) "cafebabe"
float(3.14)
bool(true)
bool(false)
NULL
array(0) {
}
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
array(3) {
  ["a"]=>
  int(1)
  ["b"]=>
  int(2)
  ["c"]=>
  int(3)
}
string(8) "stdClass"
string(10) "ValueError"
string(14) "AssertionError"
int(1)
int(2)
int(-1)
int(42)
Done
