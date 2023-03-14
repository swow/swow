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

fwrite(STDOUT, getmypid() . PHP_EOL);
Swow\Coroutine::yield();
fwrite(STDERR, 'Never here' . PHP_EOL);
