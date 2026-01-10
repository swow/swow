--TEST--
swow_closure: property hooks
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
needs_php_version('>=', '8.4');
?>
--INI--
swow.closure_serializer=1
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

class SomeClass
{
    public $a1;
    public string $p1 {
        set {
            $this->a1 = static fn() => $value;
            $this->p1 = "p1 is set to {$value}";
        }
    }

    public $a2;
    public string $p2 {
        get {
            $this->a2 = static fn() => 'whatever a2';
            return 'getting p2';
        }
    }

    public $a3;
    public string $p3 {
        set(string $v) {
            $this->a3 = static fn() => $v;
            $this->p3 = "p3 is set to {$v}";
        }
    }

    // public $a4;
    // public string $p4
    // {
    //     get () {
    //         $a4 = static fn () => 'whatever a4';
    //         return "getting p4";
    //     }
    // }

    public $a5;

    public function setA5(string $value): string
    {
        $this->a5 = static fn() => $value;
        return "a5 is set to {$value}";
    }
    public string $p5 {
        set => $this->setA5($value);
    }

    public $a6;

    public function getA6(): string
    {
        $this->a6 = static fn() => 'whatever a6';
        return 'getting p6';
    }
    public string $p6 {
        get => $this->getA6();
    }

    public $a7 {
        set($v) {
            $this->a7 = static fn() => $v;
        }
    }

    // public $a8
    // {
    //     get {
    //         return static fn () => 'whatever a8';
    //     }
    // }

    public $a9 {
        set {
            $this->a9 = static fn() => $value;
        }
    }

    public $a10 {
        get {
            return static fn() => 'whatever a10';
        }
    }

    public $a11 {
        set => static fn() => $value;
    }

    public $a12 {
        get => static fn() => 'whatever p12';
    }
}

// before serialization
$sc = new SomeClass();
$sc->p1 = 'p1';
$a = $sc->a1;
var_dump($a());

var_dump($sc->p2);
$a = $sc->a2;
var_dump($a());

$sc->p3 = 'p3';
$a = $sc->a3;
var_dump($a());

// $sc->p4 = "p4";
// $a = $sc->a4; var_dump($a());

$sc->p5 = 'p5';
$a = $sc->a5;
var_dump($a());

var_dump($sc->p6);
$a = $sc->a6;
var_dump($a());

$sc->a7 = 'a7';
$a = $sc->a7;
var_dump($a());

// var_dump($sc->a8);
// $a = $sc->a8; var_dump($a());

$sc->a9 = 'a9';
$a = $sc->a9;
var_dump($a());

// var_dump($sc->a10);
$a = $sc->a10;
var_dump($a());

$sc->a11 = 'a11';
$a = $sc->a11;
var_dump($a());

// var_dump($sc->a12);
$a = $sc->a12;
var_dump($a());

// after serialization
printf("----\n");
$serialized = serialize($sc->a1);
$unserialized = unserialize($serialized);
var_dump($unserialized());
$serialized = serialize($sc->a2);
$unserialized = unserialize($serialized);
var_dump($unserialized());
$serialized = serialize($sc->a3);
$unserialized = unserialize($serialized);
var_dump($unserialized());
// $serialized = serialize($sc->a4);
// $unserialized = unserialize($serialized);
// var_dump($unserialized());
$serialized = serialize($sc->a5);
$unserialized = unserialize($serialized);
var_dump($unserialized());
$serialized = serialize($sc->a6);
$unserialized = unserialize($serialized);
var_dump($unserialized());
$serialized = serialize($sc->a7);
$unserialized = unserialize($serialized);
var_dump($unserialized());
// $serialized = serialize($sc->a8);
// $unserialized = unserialize($serialized);
// var_dump($unserialized());
$serialized = serialize($sc->a9);
$unserialized = unserialize($serialized);
var_dump($unserialized());
$serialized = serialize($sc->a10);
$unserialized = unserialize($serialized);
var_dump($unserialized());
$serialized = serialize($sc->a11);
$unserialized = unserialize($serialized);
var_dump($unserialized());
$serialized = serialize($sc->a12);
$unserialized = unserialize($serialized);
var_dump($unserialized());

printf("Done\n");
?>
--EXPECTF--
string(2) "p1"
string(10) "getting p2"
string(11) "whatever a2"
string(2) "p3"
string(2) "p5"
string(10) "getting p6"
string(11) "whatever a6"
string(2) "a7"
string(2) "a9"
string(12) "whatever a10"
string(3) "a11"
string(12) "whatever p12"
----
string(2) "p1"
string(11) "whatever a2"
string(2) "p3"
string(2) "p5"
string(11) "whatever a6"
string(2) "a7"
string(2) "a9"
string(12) "whatever a10"
string(3) "a11"
string(12) "whatever p12"
Done
