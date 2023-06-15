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

use Psr\Http\Message\MessageInterface;
use Psr\Http\Message\StreamInterface;

interface MessagePlusInterface extends MessageInterface
{
    public const DEFAULT_PROTOCOL_VERSION = '1.1';

    public function getProtocolVersion(): string;

    public function setProtocolVersion(string $version): static;

    public function withProtocolVersion(mixed $version): static;

    public function hasHeader(mixed $name): bool;

    /** @return string[] $headers */
    public function getHeader(mixed $name): array;

    public function getHeaderLine(mixed $name): string;

    public function setHeader(string $name, mixed $value): static;

    public function withHeader(mixed $name, mixed $value): static;

    public function addHeader(string $name, mixed $value): static;

    public function withAddedHeader(mixed $name, mixed $value): static;

    public function unsetHeader(string $name): static;

    public function withoutHeader(mixed $name): static;

    /** @return array<string, array<string>> $headers */
    public function getHeaders(): array;

    /**
     * TODO: consider whether it's needed
     * @return array<string, array<string>>
     */
    public function getStandardHeaders(): array;

    /** @param array<string, array<string>|string> $headers */
    public function setHeaders(array $headers): static;

    /** @param array<string, array<string>|string> $headers */
    public function withHeaders(array $headers): static;

    public function shouldKeepAlive(): bool;

    public function getBody(): StreamInterface;

    public function setBody(StreamInterface $body): static;

    public function withBody(StreamInterface $body): static;

    public function toString(bool $withoutBody = false): string;
}
