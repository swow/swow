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

trait ConfigTrait
{
    protected int $maxHeaderLength = 8192;

    protected int $maxContentLength = 8 * 1024 * 1024;

    public function getMaxHeaderLength(): int
    {
        return $this->maxHeaderLength;
    }

    /** @return $this */
    public function setMaxHeaderLength(int $maxHeaderLength): static
    {
        $this->maxHeaderLength = $maxHeaderLength;

        return $this;
    }

    public function getMaxContentLength(): int
    {
        return $this->maxContentLength;
    }

    /** @return $this */
    public function setMaxContentLength(int $maxContentLength): static
    {
        $this->maxContentLength = $maxContentLength;

        return $this;
    }
}
