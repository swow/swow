--TEST--
swow_channel: gc (circular reference) (debug only)
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Channel;

$foo = new class
{
    public Channel $channel;

    public function __construct()
    {
        $this->channel = new Channel(1);
        $this->channel->push($this);
    }
};

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done
