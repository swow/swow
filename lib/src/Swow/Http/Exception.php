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

class Exception extends \Swow\Exception
{
    /**
     * @var RawRequest|RawResponse
     */
    protected $rawData;

    public function __construct(int $statusCode, string $reasonPhrase = '')
    {
        parent::__construct($reasonPhrase ?: Status::getReasonPhrase($statusCode), $statusCode);
    }

    /**
     * @param RawRequest|RawResponse $rawData
     * @return $this
     */
    public function setRawData($rawData)
    {
        $this->rawData = $rawData;

        return $this;
    }

    /**
     * @return RawRequest|RawResponse
     */
    public function getRawData()
    {
        return $this->rawData;
    }
}
