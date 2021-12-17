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

namespace Swow\Util;

use function ob_get_clean;
use function ob_start;
use function var_dump;

function var_dump_return(...$expressions): string
{
    ob_start();

    var_dump(...$expressions);

    return ob_get_clean();
}
