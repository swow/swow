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

use Swow\Coroutine;
use Swow\Errno;
use Swow\Socket;
use Swow\SocketException;

$server = (new Socket(Socket::TYPE_TCP))
    ->bind('127.0.0.1', 9764)->listen()
    ->setReadTimeout(3000);
echo "$ telnet 127.0.0.1 9764\n\n";
while (true) {
    Coroutine::run(static function (Socket $connection): void {
        echo "No.{$connection->getFd()} established" . PHP_EOL;
        $buffer = new Swow\Buffer();
        try {
            while (true) {
                $length = $connection->recv($buffer);
                if ($length === 0) {
                    break;
                }
                echo "No.{$connection->getFd()} say: \"" . addcslashes($buffer->toString(), "\r\n") . '"' . PHP_EOL;
                $connection->send($buffer, $length);
            }
            echo "No.{$connection->getFd()} closed" . PHP_EOL;
        } catch (SocketException $exception) {
            if ($exception->getCode() === Errno::ETIMEDOUT) {
                $connection->sendString("Server has kicked you out\r\n");
            }
            echo "No.{$connection->getFd()} goaway! {$exception->getMessage()}" . PHP_EOL;
        }
    }, $server->accept());
}
