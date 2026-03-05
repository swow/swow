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

namespace Swow\Psr7\Message;

final class EventStreamEvent
{
    public function __construct(
        public string $event = 'message',
        public string $data = '',
        public ?string $id = null,
        public ?int $retry = null,
    ) {
    }
}
