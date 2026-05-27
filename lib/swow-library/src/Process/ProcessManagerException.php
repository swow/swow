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
 * swow-library 层的进程异常
 *
 * 注意：ext 层已有 \Swow\Process\ProcessException（C 扩展注册）。
 * 本类继承自它，供 library 层使用。
 */
class ProcessManagerException extends ProcessException
{
}
