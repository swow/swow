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
        'version' => '1.4.1',
    ],
    'swow-library' => [
        /* No changes were made to the library during the upgrade
         * from version 1.4.0 to 1.4.1. */
        'version' => '1.4.0',
        /* library of v1.4.1 truly only required ^v1.4.0 extension,
         * because just some fixes were made to the extension. */
        'required_extension_version' => '^1.4.0',
    ],
];
