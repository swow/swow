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

require __DIR__ . '/../tools.php';
require __DIR__ . '/DependencyManager.php';

use Swow\Tools\DependencyManager;
use function Swow\Tools\error;
use function Swow\Tools\log;
use function Swow\Tools\notice;
use function Swow\Tools\ok;

$workSpace = __DIR__ . '/../../ext/deps';

$syncFromGit = function (string $orgName, string $repoName, string $sourceDir, string $targetDir, array $requires = []): array {
    $fullName = "{$orgName}/{$repoName}";
    log("Sync {$fullName}...");
    $url = null;
    for ($baseLevel = 1; $baseLevel < 10; $baseLevel++) {
        $path = dirname(__DIR__, $baseLevel);
        if (file_exists("{$path}/.git")) {
            for ($level = $baseLevel + 1; $level < $baseLevel + 3; $level++) {
                $path = dirname(__DIR__, $level) . '/' . $repoName;
                if (file_exists("{$path}/.git")) {
                    $url = "file://{$path}";
                    break;
                }
            }
            break;
        }
    }
    if ($url === null) {
        $url = "git@github.com:{$fullName}.git";
    }
    $version = DependencyManager::sync($repoName, $url, $sourceDir, $targetDir, $requires);
    ok("Sync {$fullName} to {$version}");
    return [$repoName => "* {$fullName}@{$version}\n"];
};

try {
    $deps = [];
    $deps += $syncFromGit(
        'libcat', 'libcat',
        'libcat', "{$workSpace}/libcat",
        ['include', 'src', 'deps', 'tools', 'clean.sh', 'LICENSE']
    );
} catch (Exception $exception) {
    error($exception->getMessage());
}

$depNames = implode(', ', array_keys($deps));
$depsInfo = implode("\n", $deps);
notice('You can put following content to git commit message:');
log("Sync deps: {$depNames}\n\n{$depsInfo}");
