--TEST--
swow_channel: hasConsumers hasProducers
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Channel;
use Swow\ChannelException;
use Swow\Coroutine;

class Consumer
{
    private int $consumed = 0;

    private Channel $chan;

    private Channel $sync_chan;

    private bool $running = false;

    public function __construct(Channel $chan)
    {
        $this->sync_chan = new Channel();
        $this->chan = $chan;
    }

    public function consume()
    {
        $this->running = true;
        Coroutine::run(function () {
            while ($this->running) {
                try {
                    $good = $this->chan->pop();
                } catch (ChannelException $e) {
                    return;
                }
                pseudo_random_sleep();
                Assert::same($good, 'good');
                $this->consumed++;
            }
            // tell main we exited
            $this->sync_chan->push(1);
        });
    }

    public function stop()
    {
        $this->running = false;
        // wait coro
        $this->sync_chan->pop();
        Assert::greaterThan($this->consumed, 0);
    }
}

class Producer
{
    private int $produced = 0;

    private Channel $chan;

    private Channel $sync_chan;

    private bool $running = false;

    public function __construct($chan)
    {
        $this->sync_chan = new Channel(0);
        $this->chan = $chan;
    }

    public function produce()
    {
        $this->running = true;
        Coroutine::run(function () {
            while ($this->running) {
                try {
                    $this->chan->push('good');
                } catch (ChannelException $e) {
                    return;
                }
                $this->produced++;
            }
            // tell main we exited
            $this->sync_chan->push(1);
        });
    }

    public function stop()
    {
        $this->running = false;
        // wait coro
        $this->sync_chan->pop();
        Assert::greaterThan($this->produced, 0);
    }
}

$chan = new Channel(2);

$con = new Consumer($chan);
$pro = new Producer($chan);

$con->consume();

try {
    // nobody is producing yet
    Assert::true($chan->hasConsumers());
    Assert::false($chan->hasProducers());
} catch (Throwable $_) {
    $chan->close();
    exit(-1);
}
$pro->produce();

// mock we ran some moment
msleep(100);

// now we exit
$con->stop();

try {
    // nobody is consuming now
    Assert::false($chan->hasConsumers());
    Assert::true($chan->hasProducers());
} finally {
    $chan->close();
}

echo "Done\n";
?>
--EXPECT--
Done
