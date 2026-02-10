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

namespace Swow\Debug\Debugger;

use Exception;
use Swow\Channel;
use Swow\ChannelException;
use Swow\Coroutine;
use Swow\SocketException;
use Swow\Stream\EofStream;
use Throwable;

use function is_array;
use function sleep;
use function Swow\defer;

class DebuggerIoEofStream implements DebuggerIoInterface
{
    protected Coroutine $serverCoroutine;
    protected Coroutine $connectionCoroutine;
    protected ?EofStream $connection = null;
    protected Channel $messageChannel;

    public function __construct(string $name, int $port, string $greeting)
    {
        $this->messageChannel = new Channel();
        $server = new EofStream();
        $server->bind($name, $port)->listen();
        $this->serverCoroutine = Coroutine::run(function () use ($server, $greeting): void {
            $errorCount = 0;
            while (true) {
                try {
                    $connection = $server->accept();
                    try {
                        if (isset($this->connectionCoroutine) && $this->connectionCoroutine->isExecuting()) {
                            $connection->sendMessage($greeting);
                            $connection->sendMessage("Error: Debugger is already in use, please check if there is another client debugging\n");
                            $connection->close();
                        }
                    } catch (Throwable) {
                    }
                    $this->connection = $connection;
                    $this->connectionCoroutine = Coroutine::run(function () use ($connection, $greeting): void {
                        defer(function () use ($connection): void {
                            $this->connection = null;
                            $connection->close();
                        });
                        try {
                            $connection->sendMessage($greeting);
                            $connection->send("\r> ");
                            while (true) {
                                $data = $connection->recvMessageString();
                                $this->messageChannel->push($data);
                            }
                        } catch (SocketException|ChannelException) {
                            $connection->close();
                        }
                    });
                    // reset error count when success
                    $errorCount = 0;
                } catch (Exception $exception) {
                    // we will throw exception for continuous errors
                    if (++$errorCount > 10) {
                        throw $exception;
                    }
                    sleep(1);
                }
            }
        });
    }

    public function in(): string
    {
        if (isset($this->connection)) {
            $this->connection->send("\r> ");
        }
        return $this->messageChannel->pop();
    }

    public function out(string|array $data): static
    {
        if (is_array($data)) {
            $this->connection->sendMessageChunks($data);
        } else {
            $this->connection->sendMessage($data);
        }
        return $this;
    }

    public function error(string|array $data): static
    {
        return $this->out($data);
    }

    public function isTty(): bool
    {
        return false;
    }

    public function quit(): void
    {
        $this->out("Bye!\n");
        $this->messageChannel->close();
        $this->messageChannel = new Channel();
        $this->connection->close();
    }
}
