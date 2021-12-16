<?php
/**
 * This file is part of Swow
 *
 * @link     https://github.com/swow/swow
 * @contact  twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

namespace Swow\Stream;

use Swow\Pack\Format;
use Swow\Socket;
use function json_decode;
use function json_encode;

class JsonStream extends LengthStream
{
    public function __construct(int $type = Socket::TYPE_TCP)
    {
        parent::__construct(Format::UINT32_BE, $type);
    }

    public function recvJson(?bool $associative = true, int $depth = 512, int $flags = 0, ?int $timeout = null): array
    {
        $jsonString = $this->recvMessageString($timeout);

        return json_decode($jsonString, $associative, $depth, $flags);
    }

    /** @return $this */
    public function sendJson(array $json, int $flags = 0, int $depth = 512, ?int $timeout = null)
    {
        return $this->sendMessageString(json_encode($json, $flags, $depth), $timeout);
    }
}
