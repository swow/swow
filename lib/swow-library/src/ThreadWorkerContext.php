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

namespace Swow;

final class ThreadWorkerContext
{
    public function __construct(
        private int $id,
        private int $threadId,
        private int $managerThreadId,
        private int $workerCount,
    ) {
    }

    public function getId(): int
    {
        return $this->id;
    }

    public function getThreadId(): int
    {
        return $this->threadId;
    }

    public function getManagerThreadId(): int
    {
        return $this->managerThreadId;
    }

    public function getWorkerCount(): int
    {
        return $this->workerCount;
    }
}
