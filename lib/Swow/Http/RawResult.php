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

class RawResult
{
    /**
     * @var string[][]
     */
    public $headers = [];

    /**
     * @var Buffer
     */
    public $body;

    /**
     * @var string
     */
    public $protocolVersion = '';

    /**
     * @var array
     */
    public $headerNames = [];

    /**
     * @var int
     */
    public $contentLength = 0;

    /**
     * @var bool
     */
    public $shouldKeepAlive = false;

    /**
     * @var bool
     */
    public $isUpgrade = false;
}
