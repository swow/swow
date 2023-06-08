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

use function Swow\Utils\error;

require __DIR__ . '/autoload.php';

$addUrl = filter_var(getenv('URL'), FILTER_VALIDATE_BOOL);
$gptMode = filter_var(getenv('GPT'), FILTER_VALIDATE_BOOL);

$authorMap = [
    'twosee' => 'twose',
    'dixyes' => 'dixyes',
    'codinghuang' => 'huanghantao',
    'pandaLIU' => 'PandaLIU-1111',
    '宣言就是Siam' => 'xuanyanwow',
    'DuxWeb' => 'duxphp',
];

$versions = require __DIR__ . '/versions.php';
$versionId = preg_match('/\d+\.\d+\.\d+/', $versions['swow-extension']['version'], $match);
if ($versionId !== 1) {
    error('Invalid version');
}
$versionId = $match[0];

$workspace = dirname(__DIR__);
$gitLog = trim((string) shell_exec(<<<SHELL
cd {$workspace} && git log --no-merges --pretty=format:'%H%n%aN%n%aE%n%aD%n%s%n' $(git describe --tags --abbrev=0)..HEAD
SHELL
));

if (empty($gitLog)) {
    error('No commits found or error occurred');
}

$commits = array_map(static function (string $commit) use ($authorMap) {
    $parts = explode("\n", $commit, 5);
    if (count($parts) !== 5) {
        error(sprintf("Invalid commit: '%s'", addcslashes($commit, "\n")));
    }
    $commit = [];
    $commit['hash'] = $parts[0];
    $commit['author'] = $authorMap[$parts[1]] ?? throw new RuntimeException(sprintf("Unknown author: '%s'", $parts[1]));
    $commit['email'] = $parts[2];
    $commit['date'] = $parts[3];
    $commit['message'] = addcslashes($parts[4], '\\*');
    return $commit;
}, explode("\n\n", $gitLog));

$changes = [];
foreach ($commits as $commit) {
    ['message' => $message, 'hash' => $hash, 'author' => $author] = $commit;
    if (preg_match('/ \(#(?<pull_id>\d+)\)$/', $message, $match)) {
        $link = $addUrl ?
            sprintf('[#%s](https://github.com/swow/swow/pull/%s)', $match['pull_id'], $match['pull_id']) :
            sprintf('#%s', $match['pull_id']);
        $message = substr($message, 0, -strlen($match[0]));
    } else {
        $link = $addUrl ?
            sprintf('[%s](https://github.com/swow/swow/commit/%s)', substr($hash, 0, 7), $hash) :
            sprintf('%s', substr($hash, 0, 7));
    }
    $messageLC = strtolower($message);
    if (str_starts_with($messageLC, 'back to dev')) {
        continue;
    }
    if (str_contains($messageLC, 'fix cs') ||
        str_contains($messageLC, 'code format')) {
        continue;
    }
    if (str_starts_with($messageLC, 'sync deps')) {
        $kind = 'deps';
    } elseif ($gptMode) {
        $kind = 'unclassified';
    } else {
        if (preg_match('/\btests?\b/', $messageLC) > 0) {
            $kind = 'enhanced';
        } elseif (str_starts_with($messageLC, 'fix ')) {
            $kind = 'fixed';
        } elseif (str_starts_with($messageLC, 'add ')) {
            $kind = 'new';
        } elseif (str_starts_with($messageLC, 'remove ')) {
            $kind = 'removed';
        } elseif (preg_match('/\b(?:new\b|add|support\b)/', $messageLC) > 0) {
            $kind = 'new';
        } elseif (preg_match('/\b(?:remove|deprecate)/', $messageLC) > 0) {
            $kind = 'removed';
        } elseif (preg_match('/\b(?:fix|workaround\b)/', $messageLC) > 0) {
            $kind = 'fixed';
        } else {
            $kind = 'enhanced';
        }
    }

    $changes[$kind][] = sprintf(
        '%s %s (%s) (%s)',
        match ($kind) {
            'new', 'enhanced' => '+',
            'removed' => '-',
            default => '*',
        },
        $message,
        $link,
        $addUrl ?
            sprintf('[@%s](https://github.com/%s)', $author, $author) :
            sprintf('@%s', $author),
    );
}

echo "\n";
if ($gptMode) {
    $maxCount = 10;
    $groups = array_chunk($changes['unclassified'], $maxCount);
    $groups = array_map(static function (array $group) {
        return implode("\n", $group);
    }, $groups);
    foreach ($groups as $group) {
        echo <<<TEXT
You need to interpret the following {$maxCount} Git commits, classify them and return them to me in the following JSON format:
`{"feature": ["Support PDO...", "* New API: ...", ...], "enhancement": [...], "fixed": [...], "removed": [...]}`，
Don't miss any commit, make sure there are {$maxCount} records!
Don't miss the @human part!
Don't return according to the format!
Don't make any explanation! I only need JSON!

Git commits：
```
{$group}
```

===============================================================================
TEXT, "\n\n";
    }
} else {
    $date = date('Y-m-d');
    echo <<<MARKDOWN
# [v{$versionId}](https://github.com/swow/swow/releases/tag/v{$versionId})

> release-date: {$date}

> 请随便写点什么有的没的。
>
> Please input a nonsense.


MARKDOWN;
    $changes = array_map(static function (array $group) {
        return implode("\n", $group);
    }, $changes);
    echo <<<MARKDOWN
## 🐣 What's New

{$changes['new']}

## ✨ What's Enhanced

{$changes['enhanced']}

## 🐛 What's Fixed

{$changes['fixed']}

## 👻 What's Removed

{$changes['removed']}

MARKDOWN;
}
