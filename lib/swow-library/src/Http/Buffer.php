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

use Psr\Http\Message\StreamInterface;
use RuntimeException;
use Swow\Stream\Buffer as StreamBuffer;

class Buffer extends StreamBuffer implements StreamInterface
{
    /** @return never */
    public function detach(): void
    {
        throw new RuntimeException('Can not detach a BufferStream');
    }

    public function getMetadata($key = null)
    {
        $metadata = $this->__debugInfo();

        return $key === null ? $metadata : $metadata[$key];
    }
}
