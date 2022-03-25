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

class RawRequest extends RawResult
{
    public string $uri = '';

    public string $method = '';

    public bool $isUpgrade = false;

    /** @var RawUploadedFile[] */
    public array $uploadedFiles = [];

    /** @var array{'remote_addr': string, 'remote_port': int} */
    public array $serverParams = [];
}
