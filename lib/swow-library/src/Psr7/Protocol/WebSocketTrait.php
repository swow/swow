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

namespace Swow\Psr7\Protocol;

use Swow\Psr7\Message\WebSocketFrame;
use Swow\Psr7\Message\WebSocketFrameInterface;
use Swow\WebSocket\WebSocket;

trait WebSocketTrait
{
    public function sendWebSocketFrame(WebSocketFrameInterface $frame): static
    {
        $this->getSocket()->write([
            $frame->toString(true),
            (string) $frame->getPayloadData(),
        ]);

        return $this;
    }

    public function sendWebSocketPingFrame(): static
    {
        $this->getSocket()->send(WebSocket::PING_FRAME);

        return $this;
    }

    public function sendWebSocketPongFrame(): static
    {
        $this->getSocket()->send(WebSocket::PONG_FRAME);

        return $this;
    }

    public function recvWebSocketFrame(): WebSocketFrameInterface
    {
        $frameEntity = $this->recvWebSocketFrameEntity();
        $frame = new WebSocketFrame();
        $frame->write(0, $frameEntity);
        if ($frameEntity->payloadData) {
            $frame->setPayloadData($frameEntity->payloadData);
        }

        return $frame;
    }
}
