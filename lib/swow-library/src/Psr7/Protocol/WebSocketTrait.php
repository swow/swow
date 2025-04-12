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

trait WebSocketTrait
{
    public function sendWebSocketFrame(WebSocketFrameInterface $frame, ?int $timeout = null): static
    {
        return $this->write(
            [
                $frame->toString(true),
                (string) $frame->getPayloadData(),
            ],
            $timeout
        );
    }

    public function recvWebSocketFrame(?int $timeout = null): WebSocketFrameInterface
    {
        $frameEntity = $this->recvWebSocketFrameEntity($timeout);
        $frame = new WebSocketFrame();
        $frame->write(0, $frameEntity);
        if ($frameEntity->payloadData) {
            $frame->setPayloadData($frameEntity->payloadData);
        }

        return $frame;
    }
}
