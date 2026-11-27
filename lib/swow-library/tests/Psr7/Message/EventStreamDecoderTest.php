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

namespace Swow\Tests\Psr7\Message;

use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\Attributes\DataProvider;
use PHPUnit\Framework\TestCase;
use Swow\Psr7\Message\BufferStream;
use Swow\Psr7\Message\EventStreamDecoder;
use Swow\Psr7\Message\EventStreamEvent;
use Swow\Psr7\Psr7;
use ValueError;

use function chr;

/**
 * @internal
 */
#[CoversClass(EventStreamDecoder::class)]
#[CoversClass(Psr7::class)]
final class EventStreamDecoderTest extends TestCase
{
    public function testDecodeSingleDataEvent(): void
    {
        $stream = new BufferStream("data: hello\n\n");
        $events = iterator_to_array(Psr7::readEventStream($stream), false);

        $this->assertCount(1, $events);
        $this->assertSame('message', $events[0]->event);
        $this->assertSame('hello', $events[0]->data);
        $this->assertNull($events[0]->id);
        $this->assertNull($events[0]->retry);
    }

    public function testDecodeEventWithFieldsAcrossSmallChunks(): void
    {
        $stream = new BufferStream(": ping\nid: 42\nevent: update\ndata: line1\ndata: line2\nretry: 1500\n\n");
        $events = iterator_to_array(Psr7::readEventStream($stream, 3), false);

        $this->assertCount(1, $events);
        $this->assertSame('update', $events[0]->event);
        $this->assertSame("line1\nline2", $events[0]->data);
        $this->assertSame('42', $events[0]->id);
        $this->assertSame(1500, $events[0]->retry);
    }

    public function testDecodeWillFlushTailEventWithoutEndingBlankLine(): void
    {
        $stream = new BufferStream('data: tail');
        $events = iterator_to_array(Psr7::readEventStream($stream), false);

        $this->assertCount(1, $events);
        $this->assertSame('tail', $events[0]->data);
    }

    public function testDecodeWithInvalidReadSize(): void
    {
        $this->expectException(ValueError::class);
        iterator_to_array(Psr7::readEventStream(new BufferStream("data: x\n\n"), 0), false);
    }

    #[DataProvider('provideBomLineEndingsAndFieldWhitespaceArePreservedCases')]
    public function testBomLineEndingsAndFieldWhitespaceArePreserved(string $ending, int $readSize): void
    {
        $wire = "\xef\xbb\xbfdata: first{$ending}{$ending}: heartbeat{$ending}event: token{$ending}data:  indented{$ending}data:tail{$ending}{$ending}";
        $events = iterator_to_array(Psr7::readEventStream(new BufferStream($wire), $readSize), false);
        $this->assertCount(2, $events);
        $this->assertSame('first', $events[0]->data);
        $this->assertSame('token', $events[1]->event);
        $this->assertSame(" indented\ntail", $events[1]->data);
    }

    /** @return iterable<string, array{string, int}> */
    public static function provideBomLineEndingsAndFieldWhitespaceArePreservedCases(): iterable
    {
        foreach (["\n", "\r\n", "\r"] as $ending) {
            foreach ([1, 2, 3, 8192] as $readSize) {
                yield bin2hex($ending) . " {$readSize}" => [$ending, $readSize];
            }
        }
    }

    public function testEmptyEventFieldRestoresTheDefaultEventName(): void
    {
        $events = iterator_to_array(Psr7::readEventStream(new BufferStream("event: token\nevent:\ndata: value\n\n")), false);
        $this->assertCount(1, $events);
        $this->assertSame('message', $events[0]->event);
    }

    public function testInvalidIdAndRetryFieldsDoNotReplaceValidMetadata(): void
    {
        $wire = "id: valid\nid: invalid\0id\nretry: 1500\nretry: -1\nretry: text\nunknown: ignored\ndata\n\n";
        $events = iterator_to_array(Psr7::readEventStream(new BufferStream($wire), 1), false);
        $this->assertCount(1, $events);
        $this->assertSame('', $events[0]->data);
        $this->assertSame('valid', $events[0]->id);
        $this->assertSame(1500, $events[0]->retry);
    }

    public function testLongLineAcrossManySmallReadsIsDecodedIntact(): void
    {
        $payload = str_repeat('0123456789abcdef', 16384);
        $wire = "event: completed\ndata: {$payload}\n\ndata: next\n\n";
        $events = iterator_to_array(Psr7::readEventStream(new BufferStream($wire), 64), false);
        $this->assertCount(2, $events);
        $this->assertSame('completed', $events[0]->event);
        $this->assertSame($payload, $events[0]->data);
        $this->assertSame('next', $events[1]->data);
    }

    #[DataProvider('provideCarriageReturnSplitFromLineFeedAcrossReadsCases')]
    public function testCarriageReturnSplitFromLineFeedAcrossReads(int $readSize): void
    {
        $wire = "data: a\r\ndata: b\r\r\ndata: c\n\r\n";
        $events = iterator_to_array(Psr7::readEventStream(new BufferStream($wire), $readSize), false);
        $this->assertCount(2, $events);
        $this->assertSame("a\nb", $events[0]->data);
        $this->assertSame('c', $events[1]->data);
    }

    /** @return iterable<string, array{int}> */
    public static function provideCarriageReturnSplitFromLineFeedAcrossReadsCases(): iterable
    {
        // 8 ends the first read right after the CR of "data: a\r"; the others shift the boundary around it.
        foreach ([1, 2, 7, 8, 9, 16, 8192] as $readSize) {
            yield "read {$readSize}" => [$readSize];
        }
    }

    /**
     * @param list<int> $chunkSizes
     * @param list<array{string, string, ?string, ?int}> $expected
     */
    #[DataProvider('provideExplicitChunkBoundariesAndEmptyReadsCases')]
    public function testExplicitChunkBoundariesAndEmptyReads(string $wire, array $chunkSizes, array $expected): void
    {
        $stream = new class($wire, $chunkSizes) extends BufferStream {
            /** @param list<int> $chunkSizes 0 means an empty read */
            public function __construct(string $data, private array $chunkSizes)
            {
                parent::__construct($data);
                $this->rewind();
            }

            public function read(mixed $length): string
            {
                if ($this->chunkSizes === []) {
                    return parent::read($length);
                }
                $size = array_shift($this->chunkSizes);
                return $size === 0 ? '' : parent::read(min($size, $length));
            }
        };
        $events = iterator_to_array(Psr7::readEventStream($stream, 8192), false);
        $this->assertSame($expected, self::describeEvents($events));
    }

    /** @return iterable<string, array{string, list<int>, list<array{string, string, ?string, ?int}>}> */
    public static function provideExplicitChunkBoundariesAndEmptyReadsCases(): iterable
    {
        yield 'CR at chunk end, LF after empty reads' => [
            "data: a\r\ndata: b\r\n\r\n",
            [8, 0, 0, 1, 7, 0, 2, 2],
            [['message', "a\nb", null, null]],
        ];
        yield 'line break exactly at chunk end' => [
            "event: x\ndata: 1\n\ndata: 2\n\n",
            [9, 8, 1, 0, 8, 1],
            [['x', '1', null, null], ['message', '2', null, null]],
        ];
        yield 'long line split around CRLF' => [
            'data: ' . str_repeat('z', 5000) . "\r\n\r\n",
            [4000, 1006, 1, 0, 1, 2],
            [['message', str_repeat('z', 5000), null, null]],
        ];
        yield 'lone CR then tail without line break' => [
            "data: a\r\rdata: tail",
            [8, 0, 1, 3, 0, 20],
            [['message', 'a', null, null], ['message', 'tail', null, null]],
        ];
    }

    public function testDecodeMatchesWholeInputReference(): void
    {
        foreach (range(0, 199) as $seed) {
            $wire = self::randomWire($seed);
            $expected = self::decodeWholeInput($wire);
            foreach ([1, 2, 3, 7, 64, 4096] as $readSize) {
                $events = iterator_to_array(Psr7::readEventStream(new BufferStream($wire), $readSize), false);
                $this->assertSame($expected, self::describeEvents($events), "seed {$seed}, read size {$readSize}");
            }
        }
    }

    /**
     * @param list<EventStreamEvent> $events
     * @return list<array{string, string, ?string, ?int}>
     */
    private static function describeEvents(array $events): array
    {
        return array_map(static fn(EventStreamEvent $event): array => [$event->event, $event->data, $event->id, $event->retry], $events);
    }

    /**
     * Reference decoder that splits the whole input at once, so it does not depend on read boundaries.
     *
     * @return list<array{string, string, ?string, ?int}>
     */
    private static function decodeWholeInput(string $wire): array
    {
        /** @var list<string> $lines */
        $lines = preg_split('/\r\n|\r|\n/', $wire);
        $tail = array_pop($lines);
        if ($tail !== '') {
            $lines[] = $tail;
        }
        if ($lines !== [] && str_starts_with($lines[0], "\xef\xbb\xbf")) {
            $lines[0] = substr($lines[0], 3);
        }
        $events = [];
        $name = 'message';
        $id = null;
        $retry = null;
        $data = [];
        $lines[] = '';
        foreach ($lines as $line) {
            if ($line === '') {
                if ($data !== []) {
                    $events[] = [$name, implode("\n", $data), $id, $retry];
                }
                $name = 'message';
                $retry = null;
                $data = [];
                continue;
            }
            if ($line[0] === ':') {
                continue;
            }
            [$field, $value] = str_contains($line, ':') ? explode(':', $line, 2) : [$line, ''];
            if (str_starts_with($value, ' ')) {
                $value = substr($value, 1);
            }
            switch ($field) {
                case 'data':
                    $data[] = $value;
                    break;
                case 'event':
                    $name = $value !== '' ? $value : 'message';
                    break;
                case 'id':
                    if (!str_contains($value, "\0")) {
                        $id = $value;
                    }
                    break;
                case 'retry':
                    if ($value !== '' && ctype_digit($value)) {
                        $retry = (int) $value;
                    }
                    break;
            }
        }
        return $events;
    }

    private static function randomWire(int $seed): string
    {
        mt_srand($seed);
        $endings = ["\n", "\r\n", "\r"];
        $wire = mt_rand(0, 4) === 0 ? "\xef\xbb\xbf" : '';
        for ($event = mt_rand(0, 20); $event > 0; $event--) {
            $ending = $endings[mt_rand(0, 2)];
            if (mt_rand(0, 5) === 0) {
                $wire .= ": comment{$ending}";
            }
            if (mt_rand(0, 3) === 0) {
                $wire .= 'event: ' . (mt_rand(0, 4) ? 'delta' : '') . $ending;
            }
            if (mt_rand(0, 4) === 0) {
                $wire .= 'id: ' . $event . (mt_rand(0, 6) ? '' : "\0x") . $ending;
            }
            if (mt_rand(0, 6) === 0) {
                $wire .= 'retry: ' . (mt_rand(0, 3) ? (string) mt_rand(0, 5000) : 'x1') . $ending;
            }
            for ($line = mt_rand(0, 3); $line > 0; $line--) {
                // Mostly short values, sometimes a long one that spans many reads.
                $length = mt_rand(0, 9) === 0 ? mt_rand(1000, 20000) : mt_rand(0, 80);
                $wire .= (mt_rand(0, 8) ? 'data:' . (mt_rand(0, 1) ? ' ' : '') : 'data') . str_repeat(chr(mt_rand(0x21, 0x7E)), $length) . $endings[mt_rand(0, 2)];
            }
            $wire .= $endings[mt_rand(0, 2)];
        }
        if (mt_rand(0, 3) === 0) {
            $wire .= 'data: tail' . (mt_rand(0, 1) ? "\r" : '');
        }
        return $wire;
    }

    public function testEventIdsPersistUntilAnExplicitReset(): void
    {
        $wire = "id: first\n\ndata: one\n\ndata: two\n\nid:\ndata: three\n\nid: invalid\0id\ndata: four\n\n";
        $events = iterator_to_array(Psr7::readEventStream(new BufferStream($wire), 1), false);
        $this->assertCount(4, $events);
        $this->assertSame(['first', 'first', '', ''], array_map(static fn(EventStreamEvent $event): ?string => $event->id, $events));
    }
}
