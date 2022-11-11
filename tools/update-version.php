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

use function Swow\Utils\info;
use function Swow\Utils\success;

$versions = [
    'swow-extension' => [
        'version' => '0.3.0-alpha.1',
    ],
    'swow-library' => [
        'version' => '0.3.0-alpha.1',
        'required_extension_version' => '0.3.0.*',
    ],
];

require __DIR__ . '/autoload.php';

putenv('SKIP_SWOW_REQUIRED_EXTENSION_CHECK=1');

$convertVersionStringPartsToNumeric = static function (string $major, string $minor, string $release): int {
    return (int) (
        str_pad($major, 2, '0', STR_PAD_LEFT) .
        str_pad($minor, 2, '0', STR_PAD_LEFT) .
        str_pad($release, 2, '0', STR_PAD_LEFT)
    );
};

foreach ($versions as $key => $_versionInfo) {
    $versionInfo['version'] = $_versionInfo['version'];
    $parts = explode('-', $versionInfo['version'], 2);
    $parts = [...explode('.', $parts[0], 3), $parts[1] ?? ''];
    $versionInfo['version_id'] = $convertVersionStringPartsToNumeric($parts[0], $parts[1], $parts[2]);
    $versionInfo['major_version'] = $parts[0];
    $versionInfo['minor_version'] = $parts[1];
    $versionInfo['release_version'] = $parts[2];
    $versionInfo['extra_version'] = $parts[3];
    if (isset($_versionInfo['required_extension_version'])) {
        $versionInfo['required_extension_version'] = $_versionInfo['required_extension_version'];
    }
    $versions[$key] = $versionInfo;
}

info('Updating version info ...');

$swowHeaderPath = __DIR__ . '/../ext/include/swow.h';
$swowHeaderContents = file_get_contents($swowHeaderPath);
$swowHeaderContents = preg_replace('/(SWOW_VERSION[ ]+")[^"]+(")/', "\${1}{$versions['swow-extension']['version']}\${2}", $swowHeaderContents);
$swowHeaderContents = preg_replace('/(SWOW_VERSION_ID[ ]+)[^\n]+/', "\${1}{$versions['swow-extension']['version_id']}", $swowHeaderContents);
$swowHeaderContents = preg_replace('/(SWOW_MAJOR_VERSION[ ]+)[^\n]+/', "\${1}{$versions['swow-extension']['major_version']}", $swowHeaderContents);
$swowHeaderContents = preg_replace('/(SWOW_MINOR_VERSION[ ]+)[^\n]+/', "\${1}{$versions['swow-extension']['minor_version']}", $swowHeaderContents);
$swowHeaderContents = preg_replace('/(SWOW_RELEASE_VERSION[ ]+)[^\n]+/', "\${1}{$versions['swow-extension']['release_version']}", $swowHeaderContents);
$swowHeaderContents = preg_replace('/(SWOW_EXTRA_VERSION[ ]+")[^"]+(")/', "\${1}{$versions['swow-extension']['extra_version']}\${2}", $swowHeaderContents);
if (!@file_put_contents($swowHeaderPath, $swowHeaderContents)) {
    throw new RuntimeException(sprintf('Failed to put content to %s (%s)', $swowHeaderPath, error_get_last()['message']));
}

$libraryPhpPath = __DIR__ . '/../lib/swow-library/src/Library.php';
$libraryPhpContents = file_get_contents($libraryPhpPath);
$libraryPhpContents = preg_replace('/(VERSION[ ]+=[ ]+\')[^\']+(\')/', "\${1}{$versions['swow-library']['version']}\${2}", $libraryPhpContents);
$libraryPhpContents = preg_replace('/(VERSION_ID[ ]+=[ ]+)[^;]+/', "\${1}{$versions['swow-library']['version_id']}", $libraryPhpContents);
$libraryPhpContents = preg_replace('/(MAJOR_VERSION[ ]+=[ ]+)[^;]+/', "\${1}{$versions['swow-library']['major_version']}", $libraryPhpContents);
$libraryPhpContents = preg_replace('/(MINOR_VERSION[ ]+=[ ]+)[^;]+/', "\${1}{$versions['swow-library']['minor_version']}", $libraryPhpContents);
$libraryPhpContents = preg_replace('/(RELEASE_VERSION[ ]+=[ ]+)[^;]+/', "\${1}{$versions['swow-library']['release_version']}", $libraryPhpContents);
$libraryPhpContents = preg_replace('/(EXTRA_VERSION[ ]+=[ ]+\')[^\']+(\')/', "\${1}{$versions['swow-library']['extra_version']}\${2}", $libraryPhpContents);
$libraryPhpContents = preg_replace('/(REQUIRED_EXTENSION_VERSION[ ]+=[ ]+\')[^\']+(\')/', "\${1}{$versions['swow-library']['required_extension_version']}\${2}", $libraryPhpContents);
if (!@file_put_contents($libraryPhpPath, $libraryPhpContents)) {
    throw new RuntimeException(sprintf('Failed to put content to %s (%s)', $swowHeaderPath, error_get_last()['message']));
}

info(json_encode($versions, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE));
success('Version updated');
