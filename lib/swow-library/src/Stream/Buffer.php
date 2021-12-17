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

class Buffer extends \Swow\Buffer
{
    /**
     * @param mixed $body
     * @return $this
     */
    public static function for($body = '')
    {
        if ($body instanceof static) {
            return $body;
        }

        $buffer = new static();
        if ($body !== '') {
            $buffer->write((string) $body);
        }

        return $buffer;
    }
}
