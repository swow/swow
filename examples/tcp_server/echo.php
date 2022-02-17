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

$server = new Swow\Socket(Swow\Socket::TYPE_TCP);
$server->bind('127.0.0.1', 9764)->listen();
echo "$ telnet 127.0.0.1 9764\n\n";
while (true) {
    $connection = $server->accept();
    Swow\Coroutine::run(static function () use ($connection): void {
        echo "No.{$connection->getFd()} established" . PHP_EOL;
        $buffer = new Swow\Buffer();
        try {
            while (true) {
                $length = $connection->recv($buffer);
                if ($length === 0) {
                    break;
                }
                echo "No.{$connection->getFd()} say: \"" . addcslashes($buffer->toString(), "\r\n") . '"' . PHP_EOL;
                $connection->send($buffer);
            }
            echo "No.{$connection->getFd()} closed" . PHP_EOL;
        } catch (Swow\Socket\Exception $exception) {
            echo "No.{$connection->getFd()} goaway! {$exception->getMessage()}" . PHP_EOL;
        }
    });
}
