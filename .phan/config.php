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
    'target_php_version' => '8.0',
    'directory_list' => [
        'lib',
    ],
    'exclude_file_regex' => '@^lib/swow-stub/src/Swow.php@',
    'exclude_analysis_directory_list' => [
        'vendor/',
    ],
    'autoload_internal_extension_signatures' => [
        'swow' => 'lib/swow-stub/src/Swow.php',
    ],
];
