--TEST--
swow_event: uncleaned resources
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Socket;

$foo = new class {
    public function __destruct()
    {
        global $socket;
        $socket = new Socket;
    }
};

echo 'Done' . PHP_LF;

?>
--EXPECT--
Done
