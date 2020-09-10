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

namespace Swow\Http;

use Psr\Http\Message\StreamInterface;

class Buffer extends \Swow\Buffer implements StreamInterface
{
    public static function create($body = ''): self
    {
        if ($body instanceof self) {
            return $body;
        }

        $buffer = new static();
        if ($body != '') {
            $buffer->write((string) $body);
        }

        return $buffer;
    }

    public function detach()
    {
        throw new \RuntimeException('Can not detach a BufferStream');
    }

    public function getMetadata($key = null)
    {
        $metadata = parent::__debugInfo();

        return $key === null ? $metadata : $metadata[$key];
    }
}
