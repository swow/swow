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

(static function (): void {
    $bootstrap = __DIR__ . '/../../../ext/tests/include/bootstrap.php';
    if (file_exists($bootstrap)) {
        require $bootstrap;
    } else {
        throw new RuntimeException('Bootstrap file not found');
    }
})();
