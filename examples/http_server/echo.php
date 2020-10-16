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

use Swow\Buffer;
use Swow\Coroutine;
use Swow\Http\Parser;
use Swow\Http\Parser\Exception as ParserException;
use Swow\Socket;
use Swow\Socket\Exception as SocketException;

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
        $client = $server->accept();
    } catch (SocketException $exception) {
        break;
    }
    Coroutine::run(function () use ($client) {
        $buffer = new Buffer();
        $parser = (new Parser())->setType(Parser::TYPE_REQUEST)->setEvents(Parser::EVENT_BODY);
        $body = null;
        try {
            while (true) {
                $length = $client->recv($buffer);
                if ($length === 0) {
                    break;
                }
                while (true) {
                    $event = $parser->execute($buffer);
                    if ($event === Parser::EVENT_BODY) {
                        if ($body === null) {
                            $body = new Buffer();
                        }
                        $body->write($buffer->toString(), $parser->getDataOffset(), $parser->getDataLength());
                    }
                    if ($parser->isCompleted()) {
                        $response = sprintf(
                            "HTTP/1.1 200 OK\r\n" .
                            "Connection: %s\r\n" .
                            "Content-Length: %d\r\n\r\n" .
                            '%s',
                            $parser->shouldKeepAlive() ? 'Keep-Alive' : 'Closed',
                            $body ? $body->getLength() : 0,
                            $body ? $body->toString() : ''
                        );
                        $client->sendString($response);
                        if ($body !== null) {
                            $body->clear();
                        }
                        break;
                    }
                }
                if (!$parser->shouldKeepAlive()) {
                    break;
                }
            }
        } catch (SocketException $exception) {
            echo "No.{$client->getFd()} goaway! {$exception->getMessage()}" . PHP_EOL;
        } catch (ParserException $exception) {
            echo "No.{$client->getFd()} parse error! {$exception->getMessage()}" . PHP_EOL;
        }
        $client->close();
    });
}
