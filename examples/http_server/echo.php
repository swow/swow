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
use Swow\Http\Parser;
use Swow\Http\ParserException;
use Swow\Socket;
use Swow\SocketException;

$host = getenv('SERVER_HOST') ?: '127.0.0.1';
$port = (int) (getenv('SERVER_PORT') ?: 9764);
$backlog = (int) (getenv('SERVER_BACKLOG') ?: 8192);
$multi = (bool) (getenv('SERVER_MULTI') ?: false);
$bindFlag = Socket::BIND_FLAG_NONE;

$server = new Socket(Socket::TYPE_TCP);
if ($multi) {
    $server->setTcpAcceptBalance(true);
    $bindFlag |= Socket::BIND_FLAG_REUSEPORT;
}
$server->bind($host, $port, $bindFlag)->listen($backlog);
while (true) {
    try {
        $connection = $server->accept();
    } catch (SocketException $exception) {
        break;
    }
    Coroutine::run(static function () use ($connection): void {
        $buffer = new Buffer(Buffer::COMMON_SIZE);
        $parser = (new Parser())->setType(Parser::TYPE_REQUEST)->setEvents(Parser::EVENT_BODY);
        $parsedOffset = 0;
        $body = null;
        try {
            while (true) {
                $length = $connection->recv($buffer, $buffer->getLength());
                if ($length === 0) {
                    break;
                }
                while (true) {
                    $parsedOffset += $parser->execute($buffer, $parsedOffset);
                    if ($parser->getEvent() === $parser::EVENT_NONE) {
                        $buffer->truncateFrom($parsedOffset);
                        $parsedOffset = 0;
                        break; /* goto recv more data */
                    }
                    if ($parser->getEvent() === Parser::EVENT_BODY) {
                        $body ??= new Buffer(Buffer::COMMON_SIZE);
                        $body->write(0, $buffer, $parser->getDataOffset(), $parser->getDataLength());
                    }
                    if ($parser->isCompleted()) {
                        $response = sprintf(
                            "HTTP/1.1 200 OK\r\n" .
                            "Connection: %s\r\n" .
                            "Content-Length: %d\r\n\r\n" .
                            '%s',
                            $parser->shouldKeepAlive() ? 'Keep-Alive' : 'Closed',
                            $body ? $body->getLength() : 0,
                            $body ?: ''
                        );
                        $connection->send($response);
                        $body?->clear();
                        break; /* goto recv more data */
                    }
                }
                if (!$parser->shouldKeepAlive()) {
                    break;
                }
            }
        } catch (SocketException $exception) {
            echo "No.{$connection->getFd()} goaway! {$exception->getMessage()}" . PHP_EOL;
        } catch (ParserException $exception) {
            echo "No.{$connection->getFd()} parse error! {$exception->getMessage()}" . PHP_EOL;
        }
        $connection->close();
    });
}
