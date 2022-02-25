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

use function Swow\Debug\showExecutedSourceLines;

require __DIR__ . '/../autoload.php';

showExecutedSourceLines(true);

ini_set('memory_limit', '512M');
require __DIR__ . '/../amazing.php';
