<?php
/**
 * This file is part of Swow
 *
 * @link     https://github.com/swow/swow
 * @contact  twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

$server = (new Swow\Socket(Swow\Socket::TYPE_TCP))
    ->bind('127.0.0.1', 9764)->listen()
    ->setReadTimeout(3000);
while (true) {
    Swow\Coroutine::run(static function (Swow\Socket $client) {
        echo "No.{$client->getFd()} established" . PHP_EOL;
        $buffer = new Swow\Buffer();
        try {
            while (true) {
                $length = $client->recv($buffer);
                if ($length === 0) {
                    break;
                }
                echo "No.{$client->getFd()} say: \"" . addcslashes($buffer, "\r\n") . '"' . PHP_EOL;
                $client->send($buffer, $length);
            }
            echo "No.{$client->getFd()} closed" . PHP_EOL;
        } catch (Swow\Socket\Exception $exception) {
            if ($exception->getCode() === Swow\Errno\ETIMEDOUT) {
                $client->sendString("Server has kicked you out\r\n");
            }
            echo "No.{$client->getFd()} goaway! {$exception->getMessage()}" . PHP_EOL;
        }
    }, $server->accept());
}
