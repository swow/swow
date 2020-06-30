<?php
/**
 * This file is part of Swow
 *
 * @link     https://github.com/s-wow/swow
 * @contact  twosee <twose@qq.com>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

namespace Swow\Http;

use Swow\Http\Server\Session;
use Swow\Http\Server\WebSocketFrame;
use Swow\Socket;
use Swow\Socket\Exception as SocketException;

class Server extends Socket
{
    /* @var Session[] */
    protected $sessions = [];

    protected $maxHeaderLength = 8192;

    protected $maxContentLength = 8 * 1024 * 1024;

    public function __construct()
    {
        parent::__construct(static::TYPE_TCP);
    }

    public function acceptSession(int $timeout = null): Session
    {
        if ($timeout === null) {
            $timeout = $this->getAcceptTimeout();
        }
        /* @var $session Session */
        $session = parent::accept(new Session(), $timeout);
        if ($session) {
            $session->setServer($this);
            $this->sessions[$session->getFd()] = $session;
        }

        return $session;
    }

    public function broadcastMessage(WebSocketFrame $frame, array $targets = null)
    {
        if ($targets = null) {
            $targets = $this->sessions;
        }
        if ($frame->getPayloadLength() <= Buffer::PAGE_SIZE) {
            $frame = $frame->toString();
        }
        foreach ($targets as $target) {
            if ($target->getType() !== $target::TYPE_WEBSOCKET) {
                continue;
            }
            try {
                if (is_string($frame)) {
                    $target->sendString($frame);
                } else {
                    $target->sendWebSocketFrame($frame);
                }
            } catch (SocketException $exception) {
                /* ignore */
            }
        }
    }

    public function offline(int $fd)
    {
        unset($this->sessions[$fd]);
    }

    public function closeSessions()
    {
        foreach ($this->sessions as $session) {
            $session->close();
        }
        $this->sessions = [];
    }

    public function close(): bool
    {
        $ret = parent::close();
        $this->closeSessions();

        return $ret;
    }

    public function __destruct()
    {
        $this->closeSessions();
    }

    /* configs */

    public function getMaxHeaderLength(): int
    {
        return $this->maxHeaderLength;
    }

    public function setMaxHeaderLength(int $maxHeaderLength): self
    {
        $this->maxHeaderLength = $maxHeaderLength;

        return $this;
    }

    public function getMaxContentLength(): int
    {
        return $this->maxContentLength;
    }

    public function setMaxContentLength(int $maxContentLength): self
    {
        $this->maxContentLength = $maxContentLength;

        return $this;
    }
}
