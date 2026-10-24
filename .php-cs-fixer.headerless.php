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

// The three example demos are WTFPL, so they are formatted without the Apache
// file header that the rest of the repository carries
$config = require __DIR__ . '/.php-cs-fixer.dist.php';
$config->setRule('header_comment', false);
$config->setFinder(
    PhpCsFixer\Finder::create()
        ->name([
            '*.php',
            '*.phpt',
            '*.inc',
        ])
        ->in([
            __DIR__ . '/examples/gemm',
            __DIR__ . '/examples/mandelbrot',
            __DIR__ . '/examples/transformer',
        ])
);

return $config;
