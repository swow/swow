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

class VarStream extends LengthStream
{
    public function __construct(int $type = self::TYPE_TCP)
    {
        parent::__construct(Format::UINT32_BE, $type);
    }

    /** @param array<string, mixed> $options */
    public function recvVar(array $options = [], ?int $timeout = null): mixed
    {
        $serializedString = $this->recvMessageString($timeout);

        return unserialize($serializedString, $options);
    }

    public function sendVar(mixed $var, ?int $timeout = null): static
    {
        return $this->sendMessageString(serialize($var), $timeout);
    }
}
