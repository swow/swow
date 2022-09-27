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

use Swow\Http;
use Swow\Http\Status;

class Response extends Message implements ResponsePlusInterface
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
        $this->statusCode = $code;
        $this->reasonPhrase = $reasonPhrase ?: Status::getReasonPhraseOf($code);

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
            $this->getStatusCode(),
            $this->getStandardHeaders(),
            (!$withoutBody && $this->hasBody()) ? (string) $this->getBody() : '',
            $this->getReasonPhrase(),
            $this->getProtocolVersion()
        );
    }
}
