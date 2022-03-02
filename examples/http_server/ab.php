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

use Swow\Buffer;
use Swow\Coroutine;
use Swow\Socket;
use Swow\SocketException;

$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1', 9764)->listen();
while (true) {
    $connection = $server->accept();
    Coroutine::run(static function () use ($connection): void {
        $buffer = new Buffer();
        try {
            while (true) {
                $length = $connection->recv($buffer);
                if ($length === 0) {
                    break;
                }
                $offset = 0;
                while (true) {
                    $eof = strpos($buffer->toString(), "\r\n\r\n", $offset);
                    if ($eof === false) {
                        break;
                    }
                    $connection->sendString(
                        "HTTP/1.1 200 OK\r\n" .
                        "Connection: keep-alive\r\n" .
                        "Content-Length: 0\r\n\r\n"
                    );
                    $requestLength = $eof + strlen("\r\n\r\n");
                    if ($requestLength === $length) {
                        $buffer->clear();
                        break;
                    }  /* < */
                    $offset += $requestLength;
                }
            }
        } catch (SocketException $exception) {
            echo "No.{$connection->getFd()} goaway! {$exception->getMessage()}" . PHP_EOL;
        }
    });
}
