<?php
/**
 * This file is part of Swow
 *
 * @link    https://github.com/swow/swow
 * @contact twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

require __DIR__ . '/../../tools/autoload.php';

use Swow\Coroutine;
use Swow\Http\Status;
use Swow\Socket\Exception as SocketException;
use Swow\Stream\VarStream;
use Swow\Sync\WaitReference;
use function Swow\Util\var_dump_return;

$wr = new WaitReference();

$server = new VarStream();
$server->bind('127.0.0.1')->listen();
Coroutine::run(function () use ($server, $wr) {
    try {
        while (true) {
            $connection = $server->accept();
            Coroutine::run(function () use ($connection, $wr) {
                echo "Connection<fd={$connection->getFd()}> accepted" . PHP_EOL;
                try {
                    while (true) {
                        $variable = $connection->recvVar();
                        echo sprintf(
                            "Connection<fd=%s>: %s\n",
                            $connection->getFd(),
                            var_dump_return($variable)
                        );
                        $connection->sendVar(['code' => Status::OK, 'message' => Status::getReasonPhrase(Status::OK)]);
                    }
                } catch (SocketException $exception) {
                    echo "Connection<fd={$connection->getFd()}> goaway, reason: {$exception->getMessage()}" . PHP_EOL;
                }
            });
        }
    } catch (SocketException $exception) {
    }
});
$client = new VarStream();
$client->connect($server->getSockAddress(), $server->getSockPort());
for ($n = 0; $n < 10; $n++) {
    $variable = $client->sendVar(['data' => "Hello Swow {$n}"])->recvVar();
    echo sprintf(
        "Server<address=%s:%s>: %s\n",
        $server->getSockAddress(),
        $server->getSockPort(),
        var_dump_return($variable)
    );
    sleep(1);
}
$client->close();
$server->close();
WaitReference::wait($wr);
