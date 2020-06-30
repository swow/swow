--TEST--
swow_coroutine: gc (circular reference) (debug only)
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$foo = new class
{
    public $coroutine;

    public function __construct()
    {
        $this->coroutine = new \Swow\Coroutine(function () { });
    }
};

?>
--EXPECT--
