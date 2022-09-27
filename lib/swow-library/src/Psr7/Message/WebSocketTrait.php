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

namespace Swow\Psr7\Message;

trait WebSocketTrait
{
    public function sendWebSocketFrame(WebSocketFrame $frame): void
    {
        $this->write([
            $frame->toString(true),
            (string) $frame->getPayloadData(),
        ]);
    }

    public function recvWebSocketFrame(): WebSocketFrame
    {
        $frameEntity = $this->recvWebSocketFrameEntity();
        $frame = new WebSocketFrame();
        $frame->write(0, $frameEntity->toString());
        if ($frameEntity->payloadData) {
            $frame->setPayloadData($frameEntity->payloadData);
        }

        return $frame;
    }
}
