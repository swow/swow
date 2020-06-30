--TEST--
swow_channel: gc (circular reference) (debug only)
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$foo = new class
{
    public $channel;

    public function __construct()
    {
        $this->channel = new Swow\Channel(1);
        $this->channel->push($this);
    }
};

?>
--EXPECT--
