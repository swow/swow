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

use const PHP_INT_MAX;

trait MaxMessageLengthTrait
{
    protected int $maxMessageLength = PHP_INT_MAX;

    public function getMaxMessageLength(): int
    {
        return $this->maxMessageLength;
    }

    /** @return $this */
    public function setMaxMessageLength(int $maxMessageLength): static
    {
        if ($maxMessageLength < 0) {
            $maxMessageLength = PHP_INT_MAX;
        }
        $this->maxMessageLength = $maxMessageLength;

        return $this;
    }
}
