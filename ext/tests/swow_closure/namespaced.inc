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

namespace SomeNamespace;

function getAnonymous()
{
    return static function (): void {
        echo('im namespaced! ' . basename(__FILE__) . ':' . __NAMESPACE__) . "\n";
    };
}
