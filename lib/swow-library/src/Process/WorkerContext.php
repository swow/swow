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

namespace Swow\Process;

/**
 * Worker 进程的上下文信息
 *
 * 传递给 Worker callback，包含 Worker 标识和进程拓扑信息
 */
class WorkerContext
{
    public function __construct(
        protected int $id,
        protected int $pid,
        protected int $managerPid,
        protected int $workerCount,
    ) {
    }

    /** Worker 编号 (0-based) */
    public function getId(): int
    {
        return $this->id;
    }

    /** 当前 Worker 进程 PID */
    public function getPid(): int
    {
        return $this->pid;
    }

    /** Manager 进程 PID */
    public function getManagerPid(): int
    {
        return $this->managerPid;
    }

    /** 总 Worker 数量 */
    public function getWorkerCount(): int
    {
        return $this->workerCount;
    }
}
