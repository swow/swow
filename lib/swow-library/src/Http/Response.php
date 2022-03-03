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

namespace Swow\Http;

use Psr\Http\Message\ResponseInterface;
use Psr\Http\Message\StreamInterface;

class Response extends Message implements ResponseInterface
{
    protected int $statusCode = Status::OK;

    protected string $reasonPhrase = 'OK';

    /**
     * @param int $statusCode Status code
     * @param array<string, array<string>|string> $headers Response headers
     * @param resource|StreamInterface|string|null $body Response body
     * @param string $reasonPhrase Reason phrase (when empty string '', a default will be used based on the status code)
     * @param string $protocolVersion Protocol version
     */
    public function __construct(int $statusCode = Status::OK, array $headers = [], $body = '', string $reasonPhrase = '', string $protocolVersion = self::DEFAULT_PROTOCOL_VERSION)
    {
        if ($statusCode !== Status::OK || $reasonPhrase !== '') {
            $this->setStatus($statusCode, $reasonPhrase);
        }

        parent::__construct($headers, $body, $protocolVersion);
    }

    public function getStatusCode(): int
    {
        return $this->statusCode;
    }

    public function getReasonPhrase(): string
    {
        return $this->reasonPhrase;
    }

    public function setStatus(int $statusCode, string $reasonPhrase = ''): static
    {
        $this->statusCode = $statusCode;
        $this->reasonPhrase = $reasonPhrase ?: Status::getReasonPhraseFor($statusCode);

        return $this;
    }

    /**
     * @param int $code
     * @param string $reasonPhrase
     */
    public function withStatus($code, $reasonPhrase = ''): static
    {
        if ($reasonPhrase === '' && $code === $this->statusCode) {
            return $this;
        }

        $new = clone $this;
        $new->setStatus($code, $reasonPhrase);

        return $new;
    }

    public function toString(bool $headOnly = false): string
    {
        return packResponse(
            $this->getStatusCode(),
            $this->getStandardHeaders(),
            !$headOnly ? $this->getBodyAsString() : '',
            $this->getReasonPhrase(),
            $this->getProtocolVersion()
        );
    }
}
