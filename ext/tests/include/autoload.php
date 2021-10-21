<?php
/**
 * This file is part of Swow
 *
 * @link     https://github.com/swow/swow
 * @contact  twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

/* TODO: should we go to PHPUnit?
 * : no, we can just test extension here,
 *   and test library by phpunit */

spl_autoload_register(function (string $class) {
    $file = __DIR__ . "/lib/{$class}.php";
    if (file_exists($file)) {
        require $file;
    }
});
