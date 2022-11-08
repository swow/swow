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

foreach (
    [
        __DIR__ . '/../vendor/autoload.php',
        __DIR__ . '/../../../autoload.php',
        null,
    ] as $file
) {
    if ($file === null) {
        throw new RuntimeException('Unable to locate autoload.php');
    }
    if (file_exists($file)) {
        require $file;
        break;
    }
}
