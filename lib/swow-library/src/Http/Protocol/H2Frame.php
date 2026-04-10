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

namespace Swow\Http\Protocol;

use function ord;
use function pack;
use function strlen;
use function substr;
use function unpack;

final class H2Frame
{
    public const HEADER_LENGTH = 9;
    public const MAX_STREAM_ID = 0x7fffffff;
    public const MAX_WINDOW_SIZE = 0x7fffffff;
    public const DEFAULT_MAX_FRAME_SIZE = 16_384;

    public const TYPE_DATA = 0x00;
    public const TYPE_HEADERS = 0x01;
    public const TYPE_PRIORITY = 0x02;
    public const TYPE_RST_STREAM = 0x03;
    public const TYPE_SETTINGS = 0x04;
    public const TYPE_PUSH_PROMISE = 0x05;
    public const TYPE_PING = 0x06;
    public const TYPE_GOAWAY = 0x07;
    public const TYPE_WINDOW_UPDATE = 0x08;
    public const TYPE_CONTINUATION = 0x09;

    public const FLAG_END_STREAM = 0x01;
    public const FLAG_ACK = 0x01;
    public const FLAG_END_HEADERS = 0x04;

    public function __construct(
        public int $type,
        public int $flags,
        public int $streamId,
        public string $payload = '',
    ) {}

    public static function fromHeaderAndPayload(string $header, string $payload): static
    {
        if (strlen($header) !== self::HEADER_LENGTH) {
            throw H2Exception::forConnectionError('Invalid frame header length', H2Exception::ERROR_FRAME_SIZE_ERROR);
        }

        $length =
            (ord($header[0]) << 16) |
            (ord($header[1]) << 8) |
            ord($header[2]);
        $type = ord($header[3]);
        $flags = ord($header[4]);
        $streamId = (unpack('N', substr($header, 5, 4))[1] ?? 0) & self::MAX_STREAM_ID;

        if (strlen($payload) !== $length) {
            throw H2Exception::forConnectionError('Frame payload length mismatch', H2Exception::ERROR_FRAME_SIZE_ERROR);
        }

        return new static($type, $flags, $streamId, $payload);
    }

    public static function decodeHeader(string $header): array
    {
        if (strlen($header) !== self::HEADER_LENGTH) {
            throw H2Exception::forConnectionError('Invalid frame header length', H2Exception::ERROR_FRAME_SIZE_ERROR);
        }

        return [
            'length' => (ord($header[0]) << 16) | (ord($header[1]) << 8) | ord($header[2]),
            'type' => ord($header[3]),
            'flags' => ord($header[4]),
            'streamId' => (unpack('N', substr($header, 5, 4))[1] ?? 0) & self::MAX_STREAM_ID,
        ];
    }

    public static function encodeHeader(int $length, int $type, int $flags, int $streamId): string
    {
        if ($length < 0 || $length > 0x00ffffff) {
            throw H2Exception::forConnectionError('Frame length out of range', H2Exception::ERROR_FRAME_SIZE_ERROR);
        }

        if ($streamId < 0 || $streamId > self::MAX_STREAM_ID) {
            throw H2Exception::forConnectionError('Frame stream ID out of range');
        }

        return
            pack('C', ($length >> 16) & 0xff) .
            pack('C', ($length >> 8) & 0xff) .
            pack('C', $length & 0xff) .
            pack('C', $type & 0xff) .
            pack('C', $flags & 0xff) .
            pack('N', $streamId & self::MAX_STREAM_ID);
    }

    public function encode(): string
    {
        return self::encodeHeader(strlen($this->payload), $this->type, $this->flags, $this->streamId) . $this->payload;
    }

    public static function createWindowUpdate(int $streamId, int $increment): static
    {
        if ($streamId < 0 || $streamId > self::MAX_STREAM_ID) {
            throw H2Exception::forConnectionError('WINDOW_UPDATE stream ID out of range');
        }

        if ($increment < 1 || $increment > self::MAX_WINDOW_SIZE) {
            throw H2Exception::forConnectionError('WINDOW_UPDATE increment out of range');
        }

        return new static(
            self::TYPE_WINDOW_UPDATE,
            0,
            $streamId,
            pack('N', $increment & self::MAX_WINDOW_SIZE)
        );
    }

    public static function createRstStream(int $streamId, int $errorCode): static
    {
        if ($streamId < 1 || $streamId > self::MAX_STREAM_ID) {
            throw H2Exception::forConnectionError('RST_STREAM stream ID out of range');
        }

        return new static(
            self::TYPE_RST_STREAM,
            0,
            $streamId,
            pack('N', $errorCode)
        );
    }
}
