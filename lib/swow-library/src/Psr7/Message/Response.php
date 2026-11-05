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

use InvalidArgumentException;
use Swow\Http\Http;
use Swow\Http\Status;

use function preg_match;

class Response extends AbstractMessage implements ResponsePlusInterface
{
    protected int $statusCode = Status::OK;

    protected string $reasonPhrase = 'OK';

    public function getStatusCode(): int
    {
        return $this->statusCode;
    }

    public function getReasonPhrase(): string
    {
        return $this->reasonPhrase;
    }

    public function setStatus(int $code, string $reasonPhrase = ''): static
    {
        /* reason-phrase = 1*( HTAB / SP / VCHAR / obs-text ), so only HTAB is a
         * legal control character (RFC 9112 4) */
        if (preg_match('/[\x00-\x08\x0A-\x1F\x7F]/', $reasonPhrase)) {
            throw new InvalidArgumentException('Reason phrase must not contain a control character other than HTAB');
        }
        /* status-code = 3DIGIT (RFC 9112 4); 100..599 is the registered range */
        if ($code < 100 || $code > 599) {
            throw new InvalidArgumentException("Invalid status code: {$code}");
        }
        $this->statusCode = $code;
        /* an empty phrase falls back to the default, but "0" is a legal phrase */
        $this->reasonPhrase = $reasonPhrase === '' ? Status::getReasonPhraseOf($code) : $reasonPhrase;

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

        return (clone $this)->setStatus($code, $reasonPhrase);
    }

    public function toString(bool $withoutBody = false): string
    {
        return Http::packResponse(
            statusCode: $this->getStatusCode(),
            reasonPhrase: $this->getReasonPhrase(),
            headers: $this->getStandardHeaders(),
            body: $withoutBody ? '' : (string) $this->getBody(),
            protocolVersion: $this->getProtocolVersion()
        );
    }
}
