--TEST--
swow_channel: construct missing
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$channel = new class extends Swow\Channel {
    /** @noinspection PhpMissingParentConstructorInspection */
    public function __construct() { }
};
$channel->push(true);

echo 'Never here' . PHP_LF;

?>
--EXPECTF--
Fatal error: Uncaught Error: %s@anonymous must construct first in %s:%d
Stack trace:
#0 %s(%d): Swow\Channel->push(true)
#1 {main}
  thrown in %s on line %d
