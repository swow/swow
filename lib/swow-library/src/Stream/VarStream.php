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

namespace Swow\Stream;

use Swow\Pack\Format;

use function serialize;
use function unserialize;

/**
 * Length-prefixed stream that exchanges PHP variables through serialize().
 *
 * The payload of a received message comes from the peer and is passed to
 * unserialize() as it is. When the peer is not trusted, pass
 * ['allowed_classes' => false] or an explicit class list to recvVar(), the same
 * way unserialize() takes it: otherwise the peer decides which classes this
 * process instantiates and which __wakeup() or __destruct() methods run.
 *
 * The message length is not limited by default; use setMaxMessageLength() when
 * the peer can not be trusted to send a length it actually delivers.
 */
class VarStream extends LengthStream
{
    public function __construct(int $type = self::TYPE_TCP)
    {
        parent::__construct(Format::UINT32_BE, $type);
    }

    /**
     * @param array<string, mixed> $options unserialize() options, see the class
     *                                      documentation about allowed_classes
     */
    public function recvVar(array $options = [], ?int $timeout = null): mixed
    {
        $serializedString = $this->recvMessageString($timeout);

        return unserialize($serializedString, $options);
    }

    public function sendVar(mixed $var, ?int $timeout = null): static
    {
        return $this->sendMessage(serialize($var), timeout: $timeout);
    }
}
