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

require __DIR__ . '/../autoload.php';

use Swow\Coroutine;
use Swow\Http\Status;
use Swow\SocketException;
use Swow\Stream\JsonStream;
use Swow\Sync\WaitReference;

$wr = new WaitReference();

$server = new JsonStream();
$server->bind('127.0.0.1')->listen();
Coroutine::run(static function () use ($server, $wr): void {
    try {
        while (true) {
            $connection = $server->accept();
            Coroutine::run(static function () use ($connection, $wr): void {
                echo "Connection<fd={$connection->getFd()}> accepted" . PHP_EOL;
                try {
                    while (true) {
                        $json = $connection->recvJson();
                        echo sprintf(
                            "Connection<fd=%s>: %s\n",
                            $connection->getFd(),
                            json_encode($json, JSON_UNESCAPED_UNICODE | JSON_PRETTY_PRINT)
                        );
                        $connection->sendJson(['code' => Status::OK, 'message' => Status::getReasonPhraseFor(Status::OK)]);
                    }
                } catch (SocketException $exception) {
                    echo "Connection<fd={$connection->getFd()}> goaway, reason: {$exception->getMessage()}" . PHP_EOL;
                }
            });
        }
    } catch (SocketException) {
    }
});
$client = new JsonStream();
$client->connect($server->getSockAddress(), $server->getSockPort());
for ($n = 0; $n < 10; $n++) {
    $json = $client->sendJson(['data' => "Hello Swow {$n}"])->recvJson();
    echo sprintf(
        "Server<address=%s:%s>: %s\n",
        $server->getSockAddress(),
        $server->getSockPort(),
        json_encode($json, JSON_UNESCAPED_UNICODE | JSON_PRETTY_PRINT)
    );
    sleep(1);
}
$client->close();
$server->close();
WaitReference::wait($wr);
