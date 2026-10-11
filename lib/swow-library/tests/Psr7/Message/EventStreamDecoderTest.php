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

    public function testEventIdsPersistUntilAnExplicitReset(): void
    {
        $wire = "id: first\n\ndata: one\n\ndata: two\n\nid:\ndata: three\n\nid: invalid\0id\ndata: four\n\n";
        $events = iterator_to_array(Psr7::readEventStream(new BufferStream($wire), 1), false);
        $this->assertCount(4, $events);
        $this->assertSame(['first', 'first', '', ''], array_map(static fn(EventStreamEvent $event): ?string => $event->id, $events));
    }
}
