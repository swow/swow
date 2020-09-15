#!/usr/bin/env php -n
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

require __DIR__ . '/../tools/dm.php';

use Swow\Tools\DependencyManager;
use function Swow\Tools\error;
use function Swow\Tools\log;
use function Swow\Tools\notice;
use function Swow\Tools\ok;

$syncFromGit = function (string $orgName, string $repoName, string $sourceDir, string $targetDir, array $requires = []): array {
    $fullName = "{$orgName}/{$repoName}";
    log("Sync {$fullName}...");
    $path = dirname(__DIR__, 2) . '/' . $repoName;
    if (!file_exists("{$path}/.git")) {
        $url = "git@github.com:{$fullName}.git";
    } else {
        $url = "file://{$path}";
    }
    $version = DependencyManager::sync($repoName, $url, $sourceDir, $targetDir, $requires);
    ok("Sync {$fullName} to {$version}");
    return [$repoName => "* {$fullName}@{$version}\n"];
};

try {
    $deps = [];
    $deps += $syncFromGit(
        'libcat', 'libcat',
        'libcat', __DIR__ . '/libcat',
        ['include', 'src', 'deps', 'tools', 'clean.sh', 'LICENSE']
    );
} catch (Exception $exception) {
    error($exception->getMessage());
}

$depNames = implode(', ', array_keys($deps));
$depsInfo = implode("\n", $deps);
notice('You can put following content to git commit message:');
log("Sync deps: {$depNames}\n\n{$depsInfo}");
