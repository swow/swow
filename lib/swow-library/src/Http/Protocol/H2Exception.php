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

use RuntimeException;

class H2Exception extends RuntimeException
{
    public const ERROR_NO_ERROR = 0x00;
    public const ERROR_PROTOCOL_ERROR = 0x01;
    public const ERROR_INTERNAL_ERROR = 0x02;
    public const ERROR_FLOW_CONTROL_ERROR = 0x03;
    public const ERROR_SETTINGS_TIMEOUT = 0x04;
    public const ERROR_STREAM_CLOSED = 0x05;
    public const ERROR_FRAME_SIZE_ERROR = 0x06;
    public const ERROR_REFUSED_STREAM = 0x07;
    public const ERROR_CANCEL = 0x08;
    public const ERROR_COMPRESSION_ERROR = 0x09;
    public const ERROR_CONNECT_ERROR = 0x0a;
    public const ERROR_ENHANCE_YOUR_CALM = 0x0b;
    public const ERROR_INADEQUATE_SECURITY = 0x0c;
    public const ERROR_HTTP_1_1_REQUIRED = 0x0d;

    public function __construct(
        string $message,
        protected int $errorCode = self::ERROR_INTERNAL_ERROR,
        protected int $streamId = 0,
        protected bool $connectionLevel = true,
        int $code = 0,
        ?\Throwable $previous = null
    ) {
        parent::__construct($message, $code, $previous);
    }

    public static function forConnectionError(
        string $message,
        int $errorCode = self::ERROR_PROTOCOL_ERROR,
        ?\Throwable $previous = null
    ): static {
        return new static($message, $errorCode, 0, true, 0, $previous);
    }

    public static function forStreamError(
        int $streamId,
        string $message,
        int $errorCode = self::ERROR_PROTOCOL_ERROR,
        ?\Throwable $previous = null
    ): static {
        return new static($message, $errorCode, $streamId, false, 0, $previous);
    }

    public function getH2ErrorCode(): int
    {
        return $this->errorCode;
    }

    public function getStreamId(): int
    {
        return $this->streamId;
    }

    public function isConnectionLevel(): bool
    {
        return $this->connectionLevel;
    }
}
