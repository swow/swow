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

return (new Config())
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
            ->filter(static function (Symfony\Component\Finder\SplFileInfo $fileInfo): bool {
                $pathname = $fileInfo->getPathname();
                if (empty($GLOBALS['exclude_file_path_list'])) {
                    foreach (
                        [
                            __DIR__ . '/ext/tests/swow_closure/use.inc',
                            __DIR__ . '/ext/tests/swow_closure/use.inc.inc',
                            __DIR__ . '/ext/tests/swow_closure/multiple_ns.inc',
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
                __DIR__ . '/ext/bin/swow-builder',
            ])
    );
