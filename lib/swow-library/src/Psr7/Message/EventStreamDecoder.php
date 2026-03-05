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
use function ltrim;
use function rtrim;
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
        $consumeLine = static function (string $line) use (&$eventName, &$eventId, &$eventRetry, &$eventDataLines, $dispatchEvent): ?EventStreamEvent {
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
                $value = ltrim((string) substr($line, $separatorPos + 1), ' ');
            }

            switch ($field) {
                case 'data':
                    $eventDataLines[] = $value;
                    break;
                case 'event':
                    if ($value !== '') {
                        $eventName = $value;
                    }
                    break;
                case 'id':
                    if (strpos($value, "\0") === false) {
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

            while (($lineBreakPos = strpos($lineBuffer, "\n")) !== false) {
                $line = rtrim(substr($lineBuffer, 0, $lineBreakPos), "\r");
                $lineBuffer = (string) substr($lineBuffer, $lineBreakPos + 1);
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
