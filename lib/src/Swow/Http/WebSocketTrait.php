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

namespace Swow\Http;

use Swow\WebSocket\Frame as WebSocketFrame;

trait WebSocketTrait
{
    public function recvWebSocketFrame(WebSocketFrame $frame = null): WebSocketFrame
    {
        $frame = $frame ?? new WebSocketFrame();
        $buffer = $this->buffer;
        $expectMore = $buffer->eof();
        while (true) {
            if ($expectMore) {
                /* ltrim (data left from the last request) */
                $this->recvData($buffer);
            }
            $headerLength = $frame->unpackHeader($buffer);
            if ($headerLength === 0) {
                $expectMore = true;
                continue;
            }
            $payloadLength = $frame->getPayloadLength();
            if ($payloadLength > 0) {
                $payloadData = $frame->getPayloadData();
                $writableSize = $payloadData->getWritableSize();
                if ($writableSize < $payloadLength) {
                    $payloadData->realloc($payloadData->tell() + $payloadLength);
                }
                if (!$buffer->eof()) {
                    $copyLength = min($buffer->getReadableLength(), $payloadLength);
                    /* TODO: $payloadData->copy($buffer, $copyLength); */
                    $payloadData->write($buffer->toString(), $buffer->tell(), $copyLength);
                    $buffer->seek($copyLength, SEEK_CUR);
                    $payloadLength -= $copyLength;
                }
                if ($payloadLength > 0) {
                    $this->read($payloadData, $payloadLength);
                }
                if ($frame->getMask()) {
                    $frame->unmaskPayloadData();
                } /* else {
                    // TODO: bad frame
                } */
                $payloadData->rewind();
            }
            break;
        }

        return $frame;
    }

    public function sendWebSocketFrame(WebSocketFrame $frame)
    {
        return $this->write([
            $frame->toString(true),
            $frame->getPayloadDataAsString(),
        ]);
    }
}
