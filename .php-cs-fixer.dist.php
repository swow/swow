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
            ])
            ->in([
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
                if (preg_match('/ext\/tests\/.+\.php$/', $pathname)) {
                    return false;
                }
                return true;
            })
            ->append([
                __FILE__,
            ])
    );
