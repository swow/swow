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

return [
    'swow-extension' => [
        'version' => '1.3.0',
    ],
    'swow-library' => [
        'version' => '1.3.0',
        /* library of v1.3.0 truly only required v1.2.0 extension,
         * because extension has no changes since v1.2.0 to v1.3.0. */
        'required_extension_version' => '^1.2.0',
    ],
];
