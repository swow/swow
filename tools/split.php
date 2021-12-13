#!/usr/bin/env php
<?php

require __DIR__ . '/tools.php';

use function Swow\Tools\check;
use function Swow\Tools\error;
use function Swow\Tools\passthru;
use function Swow\Tools\success;

$repoInfo = [
    'swow-stub' => 'lib/swow-stub',
    'swow-library' => 'lib/swow-library',
    'php-stub-generator' => 'lib/php-stub-generator',
];

chdir($workspace = dirname(__DIR__));
if (strpos(`splitsh-lite --version 2>&1` ?: '', 'splitsh-lite') === false) {
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
    $sha1 = trim(`splitsh-lite --prefix={$repoPath} 2>/dev/null`);
    $status = passthru("git push {$repoName} {$sha1}:refs/heads/{$targetBranch} -f 2>&1");
    check($status === 0, "Update {$repoName}");
}

success('Done');
