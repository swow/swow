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

use Swow\PhpCsFixer\Config;

// WTFPL example projects: excluded here, formatted by .php-cs-fixer.headerless.php
$headerlessPaths = [
    __DIR__ . '/examples/gemm/',
    __DIR__ . '/examples/mandelbrot/',
    __DIR__ . '/examples/transformer/',
];

$config = new Config();
$config
    ->setHeaderComment(
        projectName: 'Swow',
        projectLink: 'https://github.com/swow/swow',
        contactName: 'twosee',
        contactMail: 'twosee@php.net'
    )->setFinder(
        PhpCsFixer\Finder::create()
            ->name([
                '*.php',
                '*.phpt',
                '*.inc',
            ])
            ->in([
                __DIR__ . '/.github',
                __DIR__ . '/.phan',
                __DIR__ . '/benchmark',
                __DIR__ . '/examples',
                __DIR__ . '/ext',
                __DIR__ . '/lib',
                __DIR__ . '/tools',
            ])
            ->notName([
                'Swow.php',
                'run-tests*.php',
            ])
            ->filter(static function (Symfony\Component\Finder\SplFileInfo $fileInfo) use ($headerlessPaths): bool {
                $pathname = $fileInfo->getPathname();
                foreach ($headerlessPaths as $headerlessPath) {
                    if (str_starts_with($pathname, $headerlessPath)) {
                        return false;
                    }
                }
                if (empty($GLOBALS['exclude_file_path_list'])) {
                    foreach (
                        [
                            __DIR__ . '/ext/tests/swow_closure/use.inc',
                            __DIR__ . '/ext/tests/swow_closure/use.inc.inc',
                            __DIR__ . '/ext/tests/swow_closure/multiple_ns.inc',
                            __DIR__ . '/ext/tests/swow_closure/namespaced2.inc',
                            __DIR__ . '/ext/tests/include/lib/CertificateGenerator.php',
                            // __wakeup to __unserialize conversion is not valid for this fixture
                            __DIR__ . '/ext/tests/swow_thread/thannel_unserialize_failure.phpt',
                        ] as $excludeFilePath
                    ) {
                        $GLOBALS['exclude_file_path_list'][] = substr($excludeFilePath, strlen(__DIR__) + strlen('/'));
                    }
                }
                foreach ($GLOBALS['exclude_file_path_list'] as $excludeFilePath) {
                    if (strrpos($pathname, $excludeFilePath) !== false) {
                        return false;
                    }
                }
                if (str_contains($pathname, 'ext/build')) {
                    return false;
                }
                if (preg_match('/ext\/tests\/.+\.php$/', $pathname)) {
                    if (strrpos($pathname, 'ext/tests/include') !== false) {
                        return true;
                    }
                    return false;
                }
                return true;
            })
            ->append([
                __FILE__,
                __DIR__ . '/.php-cs-fixer.headerless.php',
                __DIR__ . '/ext/bin/swow-builder',
                __DIR__ . '/lib/dontdie/dontdie',
            ])
    );

return $config;
