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

use Generator;
use Psr\Http\Message\StreamInterface;
use ValueError;

use function ctype_digit;
use function implode;
use function rtrim;
use function strlen;
use function strpos;
use function substr;

final class EventStreamDecoder
{
    /**
     * 把 text/event-stream 按 SSE 规则解码为事件对象。
     *
     * @return Generator<int, EventStreamEvent>
     */
    public static function decode(StreamInterface $stream, int $readSize = 8192): Generator
    {
        if ($readSize <= 0) {
            throw new ValueError('Read size must be greater than 0');
        }

        $lineBuffer = '';
        $eventName = 'message';
        $eventId = null;
        $eventRetry = null;
        $eventDataLines = [];
        $firstLine = true;
        $skipLineFeed = false;

        /* 空行是 SSE 事件分隔符：只有积累过 data 才产出事件。 */
        $dispatchEvent = static function () use (&$eventName, &$eventId, &$eventRetry, &$eventDataLines): ?EventStreamEvent {
            if ($eventDataLines === []) {
                $eventName = 'message';
                $eventId = null;
                $eventRetry = null;
                return null;
            }
            $event = new EventStreamEvent(
                event: $eventName,
                data: implode("\n", $eventDataLines),
                id: $eventId,
                retry: $eventRetry,
            );
            $eventName = 'message';
            $eventId = null;
            $eventRetry = null;
            $eventDataLines = [];
            return $event;
        };

        /* 逐行消费，支持注释行、field:value 与跨 chunk 增量拼接。 */
        $consumeLine = static function (string $line) use (&$eventName, &$eventId, &$eventRetry, &$eventDataLines, &$firstLine, $dispatchEvent): ?EventStreamEvent {
            if ($firstLine) {
                $firstLine = false;
                if (str_starts_with($line, "\xef\xbb\xbf")) {
                    $line = substr($line, 3);
                }
            }
            if ($line === '') {
                return $dispatchEvent();
            }
            if ($line[0] === ':') {
                return null;
            }

            $separatorPos = strpos($line, ':');
            if ($separatorPos === false) {
                $field = $line;
                $value = '';
            } else {
                $field = substr($line, 0, $separatorPos);
                $value = substr($line, $separatorPos + 1);
                // SSE removes exactly one optional space after the colon, not data indentation.
                if (str_starts_with($value, ' ')) {
                    $value = substr($value, 1);
                }
            }

            switch ($field) {
                case 'data':
                    $eventDataLines[] = $value;
                    break;
                case 'event':
                    $eventName = $value !== '' ? $value : 'message';
                    break;
                case 'id':
                    if (!str_contains($value, "\0")) {
                        $eventId = $value;
                    }
                    break;
                case 'retry':
                    if ($value !== '' && ctype_digit($value)) {
                        $eventRetry = (int) $value;
                    }
                    break;
            }

            return null;
        };

        while (!$stream->eof()) {
            $chunk = $stream->read($readSize);
            if ($chunk === '') {
                continue;
            }
            $lineBuffer .= $chunk;

            while ($lineBuffer !== '') {
                if ($skipLineFeed) {
                    $skipLineFeed = false;
                    if ($lineBuffer[0] === "\n") {
                        $lineBuffer = substr($lineBuffer, 1);
                    }
                }
                $lineBreakPos = strcspn($lineBuffer, "\r\n");
                if ($lineBreakPos === strlen($lineBuffer)) {
                    break;
                }
                $line = substr($lineBuffer, 0, $lineBreakPos);
                // A CR ends the line immediately; its optional LF can arrive in a later read.
                $skipLineFeed = $lineBuffer[$lineBreakPos] === "\r";
                $lineBuffer = substr($lineBuffer, $lineBreakPos + 1);
                $event = $consumeLine($line);
                if ($event !== null) {
                    yield $event;
                }
            }
        }

        if ($lineBuffer !== '') {
            $event = $consumeLine(rtrim($lineBuffer, "\r"));
            if ($event !== null) {
                yield $event;
            }
        }
        $event = $dispatchEvent();
        if ($event !== null) {
            yield $event;
        }
    }
}
