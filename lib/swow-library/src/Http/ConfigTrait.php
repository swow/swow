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

trait ConfigTrait
{
    /**
     * @var int
     */
    protected $maxHeaderLength = 8192;

    /**
     * @var int
     */
    protected $maxContentLength = 8 * 1024 * 1024;

    public function getMaxHeaderLength(): int
    {
        return $this->maxHeaderLength;
    }

    /**
     * @return $this
     */
    public function setMaxHeaderLength(int $maxHeaderLength)
    {
        $this->maxHeaderLength = $maxHeaderLength;

        return $this;
    }

    public function getMaxContentLength(): int
    {
        return $this->maxContentLength;
    }

    /**
     * @return $this
     */
    public function setMaxContentLength(int $maxContentLength)
    {
        $this->maxContentLength = $maxContentLength;

        return $this;
    }
}
