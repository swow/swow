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

use Composer\Semver\Semver;
use Swow\Extension;
use Swow\Library;

if (!class_exists(Extension::class)) {
    return;
}

if (!getenv('SKIP_SWOW_REQUIRED_EXTENSION_CHECK')) {
    if (!Semver::satisfies(Extension::VERSION, Library::REQUIRED_EXTENSION_VERSION)) {
        throw new Error(sprintf(
            '%s extension version mismatch, required: %s, actual: %s',
            Swow::class, Library::REQUIRED_EXTENSION_VERSION, Extension::VERSION
        ));
    }
}
