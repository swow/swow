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

$phpVersion = $argv[1] ?? throw new InvalidArgumentException('PHP version is required');
if (!preg_match('/^8\.[0-5]$/', $phpVersion)) {
    throw new InvalidArgumentException(sprintf('Invalid PHP version \"%s\"', $phpVersion));
}

(static function (string $phpVersion): void {
    $composerJson = file_get_contents('composer.json');
    $phpunitVersion = match ($phpVersion) {
        '8.5', '8.4', '8.3', '8.2' => '^11',
        '8.1' => '^10',
        '8.0' => '^9',
        default => throw new InvalidArgumentException("Unexpected PHP version {$phpVersion}"),
    };
    $newComposerJson = str_replace('"phpunit/phpunit": "^9|^10|^11"', "\"phpunit/phpunit\": \"{$phpunitVersion}\"", $composerJson);
    file_put_contents('composer.json', $newComposerJson);
    echo "PHP {$phpVersion}, use \"phpunit/phpunit: {$phpunitVersion}\"\n";
})($phpVersion);
