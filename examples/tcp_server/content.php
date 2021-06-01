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
    /* $client
  Swow\Socket {#170
    type: "TCP4"
    fd: 18
  timeout: array:5 [
    "dns" => -1
    "accept" => -1
    "connect" => -1
    "read" => -1
    "write" => -1
  ]
  established: true
  side: "none"
  sockname: array:2 [
     "address" => "127.0.0.1"
    "port" => 9764
  ]
  peername: array:2 [
    "address" => "127.0.0.1"
    "port" => 53315
  ]
  io_state: "idle"
}
            */
    $client = $server->accept();
    Swow\Coroutine::run(function () use ($client) {
        $buffer = new Swow\Buffer();
        try {
            while (true) {
                $length = $client->recv($buffer);
                // Get the content sent by the tcp client
                $content = $buffer->getContents();
                echo sprintf('Buffer Content: %s', $content) . PHP_EOL;
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
