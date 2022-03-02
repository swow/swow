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
use Swow\SocketException;
use Swow\Stream\EofStream;

$server = new EofStream();
$server->bind('127.0.0.1', 9764)->listen();
echo "$ telnet 127.0.0.1 9764\n\n";
while (true) {
    Coroutine::run(static function (EofStream $connection): void {
        echo "Stream<fd={$connection->getFd()}> accepted" . PHP_EOL;
        try {
            while (true) {
                $message = $connection->recvMessageString();
                echo "Stream<fd={$connection->getFd()}>: \"{$message}\"" . PHP_EOL;
                $connection->sendMessageString($message);
            }
        } catch (SocketException $exception) {
            echo "Stream<fd={$connection->getFd()}> goaway, reason: {$exception->getMessage()}" . PHP_EOL;
        }
    }, $server->accept());
}
