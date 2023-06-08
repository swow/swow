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

use function Swow\Utils\error;
use function Swow\Utils\success;

require __DIR__ . '/autoload.php';

$changelog = @file_get_contents(dirname(__DIR__) . '/CHANGELOG.md');
if ($changelog === false) {
    error(sprintf('Failed to read CHANGELOG.md, reason: %s', error_get_last()['message'] ?? 'unknown error'));
}
$changelog = preg_replace('#\[([^]]+)]\(https://github.com/[^)]+\)#', '$1', $changelog);
if (!@file_put_contents(dirname(__DIR__) . '/CHANGELOG.md', $changelog)) {
    error(sprintf('Failed to write CHANGELOG.md, reason: %s', error_get_last()['message'] ?? 'unknown error'));
}
success('CHANGELOG.md stripped');
