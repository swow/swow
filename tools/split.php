#!/usr/bin/env php
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

require __DIR__ . '/autoload.php';

use function Swow\Utils\check;
use function Swow\Utils\error;
use function Swow\Utils\info;
use function Swow\Utils\log;
use function Swow\Utils\ok;
use function Swow\Utils\passthru;
use function Swow\Utils\success;
use function Swow\Utils\warn;

$repoInfo = [
    'swow-examples' => dirname(__DIR__) . '/examples',
    'swow-extension' => dirname(__DIR__) . '/ext',
    'swow-stub' => dirname(__DIR__) . '/lib/swow-stub',
    'swow-library' => dirname(__DIR__) . '/lib/swow-library',
    'psr7-plus' => dirname(__DIR__) . '/lib/swow-library/lib/psr7-plus',
    'swow-utils' => dirname(__DIR__) . '/lib/swow-utils',
    'swow-php-cs-fixer-config' => dirname(__DIR__) . '/lib/swow-php-cs-fixer-config',
    'php-stub-generator' => dirname(__DIR__) . '/lib/php-stub-generator',
    'dontdie' => dirname(__DIR__) . '/lib/dontdie',
];

$options = getopt('', ['no-cache'], $optind);
$noCache = isset($options['no-cache']);
if ($noCache) {
    info('Clearing cache...');
    if (file_exists($splitshDb = dirname(__DIR__) . '/.git/splitsh.db')) {
        if (@unlink($splitshDb) === false) {
            warn('Failed to clear cache');
        } else {
            ok('Cache cleared');
        }
    } else {
        info('No cache found');
    }
}
$specifiedRepoName = $argv[$optind] ?? null;

$repoInfo = array_map(static function ($path) {
    return str_replace(dirname(__DIR__) . '/', '', $path);
}, $repoInfo);

if ($specifiedRepoName) {
    $specifiedRepoPath = $repoInfo[$specifiedRepoName] ?? null;
    if (!$specifiedRepoPath) {
        error("A non-existent repo '{$specifiedRepoName}' is specified");
    }
    $repoInfo = [$specifiedRepoName => $specifiedRepoPath];
}

chdir($workspace = dirname(__DIR__));
if (!str_contains(shell_exec('splitsh-lite --version 2>&1') ?: '', 'splitsh-lite')) {
    error('splitsh-lite tool is unavailable');
}

// $currentBranch = trim(`git rev-parse --abbrev-ref HEAD 2>/dev/null`);
/* Note: We always develop on ci branches, but we still want to
 * use `develop` as target-branch for the sub repos. */
$targetBranch = 'develop';
foreach ($repoInfo as $repoName => $repoPath) {
    info("Updating {$repoName}...");
    if (!is_dir("{$workspace}/{$repoPath}")) {
        error("Repo {$repoName}'s path '{$repoPath}' is unavailable");
    }
    passthru("git remote add {$repoName} git@github.com:swow/{$repoName}.git >/dev/null 2>&1");
    $sha1GetCommand = "splitsh-lite --prefix={$repoPath} 2>/dev/null";
    log("> {$sha1GetCommand}");
    $sha1 = trim((string) shell_exec($sha1GetCommand));
    if (preg_match('/^[a-fA-F0-9]{40}$/', $sha1) !== 1) {
        error("splitsh failed with [{$repoName}]({$repoPath})");
    }
    $status = passthru("git push {$repoName} {$sha1}:refs/heads/{$targetBranch} -f 2>&1");
    passthru("git remote rm {$repoName} >/dev/null 2>&1");
    check($status === 0, "Update {$repoName}");
}

success('Done');
