#!/usr/bin/env php
<?php
/**
 * This file is part of Swow
 *
 * @link     https://github.com/swow/swow
 * @contact  twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

use function Swow\Util\httpGet;
use function Swow\Util\info;
use function Swow\Util\success;

require __DIR__ . '/autoload.php';

$dbUri = 'https://raw.githubusercontent.com/nginx/nginx/master/conf/mime.types';
$sourceFilePathWithoutExtension = dirname(__DIR__) . '/lib/swow-library/src/Http/MimeType';
$indent = 4;

/* download mine types info and prepare mime map */
info('Downloading mime types info...');
$mimeInfo = httpGet($dbUri);
preg_match_all('/^[ ]+([^ ]+)[ \n]+([^;]+);$/m', $mimeInfo, $matches, PREG_SET_ORDER);
$extensionMap = [];
$mimeList = [];
foreach ($matches as $match) {
    $extensions = array_map('trim', explode(' ', $match[2]));
    foreach ($extensions as $extension) {
        $mime = trim($match[1]);
        $extensionMap[$extension] = $mime;
        $mimeList[$mime][] = $extension;
    }
}
$extensionSimplyMap = [];
foreach ($mimeList as $mime => $extensions) {
    if (count($extensions) > 1) {
        foreach ($extensions as $extension) {
            if (strtolower($extension) === strtolower(explode('/', $mime)[1] ?? '')) {
                $extensionSimplyMap[$extension] = $mime;
                continue 2;
            }
        }
    }
    foreach ($extensions as $extension) {
        $extensionSimplyMap[$extension] = $mime;
    }
}

/* read source */
$skeletonSourceFilePath = "{$sourceFilePathWithoutExtension}.skl";
$skeletonSource = @file_get_contents($skeletonSourceFilePath);
if (!$skeletonSource) {
    throw new RuntimeException(sprintf('Failed to read the content type source file (%s)', error_get_last()['message']));
}

info('update Mime source file...');

$escapeConstantName = static function (string $constantName): string {
    return str_replace(['-'], ['_'],
        (!preg_match('/^[a-zA-Z_\x80-\xff]$/', $constantName[0]) ? '_' : '') . strtoupper($constantName)
    );
};

/* update mime constants */
$constantLines = [];
$constantMimeMap = [];
foreach ($extensionSimplyMap as $extension => $mime) {
    $extension = (string) $extension;
    $mimeConstant = null;
    foreach ($extensionSimplyMap as $extension2 => $mime2) {
        if ($extension === $extension2) {
            break;
        }
        if ($mime === $mime2) {
            $mimeConstant = 'self::' . $escapeConstantName($extension2);
            break;
        }
    }
    $constantName = $escapeConstantName($extension);
    $constantLines[] = sprintf(
        '%spublic const %s = %s;',
        str_repeat(' ', $indent),
        $constantName,
        $mimeConstant ?: "'{$mime}'"
    );
    $constantMimeMap[$mime] = $constantName;
}
$constantLines = implode("\n", $constantLines);
$skeletonSource = str_replace('{{constants}}', $constantLines, $skeletonSource);

/* update mime pairs */
$mimePairLines = [];
foreach ($extensionMap as $extension => $mime) {
    $extension = (string) $extension;
    $constantName = $escapeConstantName($extension);
    if (!isset($extensionSimplyMap[$constantName])) {
        $constantName = $constantMimeMap[$mime];
    }
    $mimePairLines[] = sprintf(
        "%s'%s' => self::%s,",
        str_repeat(' ', $indent + 4),
        $extension,
        $constantName
    );
}
$mimePairLines = implode("\n", $mimePairLines);
$skeletonSource = str_replace('{{extension_map}}', $mimePairLines, $skeletonSource);

if (!@file_put_contents("{$sourceFilePathWithoutExtension}.php", $skeletonSource)) {
    throw new RuntimeException(sprintf('Failed to write the new ContentType source to file (%s)', error_get_last()['message']));
}
success('MimeType updated');
