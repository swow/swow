--TEST--
swow_coroutine: gc (circular reference) (debug only)
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

$foo = new class
{
    public Coroutine $coroutine;

    public function __construct()
    {
        $this->coroutine = new Coroutine(function () { });
    }
};

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
