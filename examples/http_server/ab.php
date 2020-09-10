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

$server = new Swow\Socket(Swow\Socket::TYPE_TCP);
$server->bind('127.0.0.1', 9764)->listen();
while (true) {
    $client = $server->accept();
    Swow\Coroutine::run(function () use ($client) {
        $buffer = new Swow\Buffer();
        try {
            while (true) {
                $length = $client->recv($buffer);
                if ($length === 0) {
                    break;
                }
                $offset = 0;
                while (true) {
                    $eof = strpos($buffer->toString(), "\r\n\r\n", $offset);
                    if ($eof === false) {
                        break;
                    }
                    $client->sendString(
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
        } catch (Swow\Socket\Exception $exception) {
            echo "No.{$client->getFd()} goaway! {$exception->getMessage()}" . PHP_EOL;
        }
    });
}
