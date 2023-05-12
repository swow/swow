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

use Psr\Http\Message\ResponseInterface;

interface ResponsePlusInterface extends MessagePlusInterface, ResponseInterface
{
    public function getStatusCode(): int;

    public function getReasonPhrase(): string;

    public function setStatus(int $code, string $reasonPhrase = ''): static;

    /**
     * @param int $code
     * @param string $reasonPhrase
     */
    public function withStatus($code, $reasonPhrase = ''): static;
}
