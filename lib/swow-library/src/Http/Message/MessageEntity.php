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

namespace Swow\Http\Message;

use Swow\Buffer;

class MessageEntity
{
    public string $protocolVersion = '1.1';

    /**
     * headers holder format like `[ 'X-Header' => [ 'value 1', 'value 2' ] ]`
     *
     * @var array<string, array<string>>
     */
    public array $headers = [];

    /**
     * headers names holder format like `[ 'x-header' => 'X-Header' ]`
     *
     * @var array<string, string>
     */
    public array $headerNames = [];

    public int $contentLength = 0;

    public bool $shouldKeepAlive = false;

    public ?Buffer $body = null;
}
