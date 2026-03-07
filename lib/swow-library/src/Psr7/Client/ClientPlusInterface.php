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

namespace Swow\Psr7\Client;

use Generator;
use Psr\Http\Client\ClientInterface;
use Psr\Http\Message\RequestInterface;
use Swow\Psr7\Message\EventStreamEvent;

interface ClientPlusInterface extends ClientInterface
{
    /**
     * 发送请求并按 SSE 协议增量读取事件流。
     *
     * @return Generator<int, EventStreamEvent>
     */
    public function sendEventStreamRequest(RequestInterface $request, ?int $timeout = null, int $readSize = 8192): Generator;
}
