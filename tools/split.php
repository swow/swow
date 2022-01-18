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

require __DIR__ . '/autoload.php';

use function Swow\Util\check;
use function Swow\Util\error;
use function Swow\Util\passthru;
use function Swow\Util\success;

$repoInfo = [
    'swow-examples' => 'examples',
    'swow-stub' => 'lib/swow-stub',
    'swow-library' => 'lib/swow-library',
    'swow-util' => 'lib/swow-util',
    'php-stub-generator' => 'lib/php-stub-generator',
];

$specifiedRepoName = $argv[1] ?? null;
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
    if (!is_dir("{$workspace}/{$repoPath}")) {
        error("Repo {$repoName}'s path '{$repoPath}' is unavailable");
    }
    passthru("git remote add {$repoName} git@github.com:swow/{$repoName}.git >/dev/null 2>&1");
    $sha1 = trim(shell_exec("splitsh-lite --prefix={$repoPath} 2>/dev/null"));
    $status = passthru("git push {$repoName} {$sha1}:refs/heads/{$targetBranch} -f 2>&1");
    passthru("git remote rm {$repoName} >/dev/null 2>&1");
    check($status === 0, "Update {$repoName}");
}

success('Done');
