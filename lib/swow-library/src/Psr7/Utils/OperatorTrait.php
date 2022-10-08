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

namespace Swow\Psr7\Utils;

use Psr\Http\Message\MessageInterface;
use Swow\Psr7\Message\MessagePlusInterface;

trait OperatorTrait
{
    /**
     * @template T of MessageInterface
     * @param T $message
     * @param array<string, array<string>|string> $headers
     * @return T
     */
    public static function setHeaders(MessageInterface $message, array $headers): MessageInterface
    {
        if ($message instanceof MessagePlusInterface) {
            $message->setHeaders($headers);
        } else {
            $message = static::withHeaders($message, $headers);
        }
        return $message;
    }

    /**
     * @template T of MessageInterface
     * @param T $message
     * @param array<string, array<string>|string> $headers
     * @return T
     */
    public static function withHeaders(MessageInterface $message, array $headers): MessageInterface
    {
        if ($message instanceof MessagePlusInterface) {
            $message = $message->withHeaders($headers);
        } else {
            foreach ($headers as $headerName => $headerValue) {
                $message = $message->withHeader($headerName, $headerValue);
            }
        }
        return $message;
    }

    /**
     * @template T of MessageInterface
     * @param T $message
     * @return T
     */
    public static function setBody(MessageInterface $message, mixed $body): MessageInterface
    {
        if ($message instanceof MessagePlusInterface) {
            $message->setBody($body);
        } else {
            $message = $message->withBody(static::createStreamFromAny($body));
        }
        return $message;
    }

    /**
     * @template T of MessageInterface
     * @param T $message
     * @return T
     */
    public static function withBody(MessageInterface $message, mixed $body): MessageInterface
    {
        return $message->withBody(static::createStreamFromAny($body));
    }
}
