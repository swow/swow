--TEST--
swow_event: uncleaned resources (static)
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Socket;

$foo = new class() {
    public function __destruct()
    {
        static $socket;
        $socket = new Socket(Socket::TYPE_TCP);
    }
};

echo "Done\n";

?>
--EXPECT--
Done
