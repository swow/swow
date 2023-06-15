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
use Swow\Object\StringableInterface;

interface MessagePlusInterface extends MessageInterface, StringableInterface
{
    public const DEFAULT_PROTOCOL_VERSION = '1.1';

    public function setProtocolVersion(string $version): static;

    public function setHeader(string $name, mixed $value): static;

    public function addHeader(string $name, mixed $value): static;

    public function unsetHeader(string $name): static;

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

    public function setBody(StreamInterface $body): static;
}
