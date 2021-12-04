--TEST--
swow_coroutine: dead lock
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
/* TODO: replace it */
skip_if_constant_not_defined('SIGTERM');
skip_if_function_not_exist('pcntl_fork');
skip_if_function_not_exist('pcntl_wait');
skip_if_function_not_exist('posix_kill');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Signal;
use Swow\Socket;

$pipePath = getRandomPipePath();
$syncServer = new Socket(Socket::TYPE_PIPE);
$syncServer->bind($pipePath)->listen();
// TODO: new sync component?
$syncClientNew = function () use ($pipePath) {
    return new class($pipePath) extends Socket {
        public function __construct(string $pipePath)
        {
            parent::__construct(Socket::TYPE_PIPE);
            $this->connect($pipePath);
        }

        public function notify(): void
        {
            $this->sendString('x');
        }
    };
};

foreach (
    [
        [
            'function' => function () use ($syncClientNew) {
                $syncClient = $syncClientNew();
                $syncClient->notify();
                Swow\Coroutine::yield();
                echo 'Never here' . PHP_LF;
            },
            'expect_status' => Signal::TERM
        ],
        [
            'function' => function () use ($syncClientNew) {
                $syncClient = $syncClientNew();
                $syncClient->notify();
                Swow\Coroutine::run(function () {
                    Swow\Coroutine::yield();
                    echo 'Never here' . PHP_LF;
                });
                $syncClient->notify();
            },
            'expect_status' => 0
        ]
    ] as $item
) {
    $child = pcntl_fork();
    if ($child < 0) {
        return;
    }
    if ($child === 0) {
        return $item['function']();
    } else {
        $syncConnection = $syncServer->accept();
        Assert::same($syncConnection->readString(1), 'x'); // wait()
        if ($item['expect_status'] === Signal::TERM) {
            Signal::kill($child, Signal::TERM);
        }
        pcntl_wait($status);
        Assert::same($status, $item['expect_status']);
    }
}
echo PHP_LF . 'Done' . PHP_LF;

?>
--EXPECTF--
Warning: <COROUTINE> Dead lock: all coroutines are asleep in scheduler

Done
