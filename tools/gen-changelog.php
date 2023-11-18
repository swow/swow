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

use Swow\Coroutine;
use Swow\Sync\WaitReference;
use function Swow\Utils\error;

require __DIR__ . '/autoload.php';

date_default_timezone_set('PRC');

$authorMap = [
    'twosee' => 'twose',
    'dixyes' => 'dixyes',
    'codinghuang' => 'huanghantao',
    'pandaLIU' => 'PandaLIU-1111',
    '宣言就是Siam' => 'xuanyanwow',
    'DuxWeb' => 'duxphp',
    'AuroraHe' => 'AuroraYolo',
    '李铭昕' => 'limingxinleo',
];
$kinds = [
    'new',
    'enhanced',
    'fixed',
    'removed',
    'deps',
    'meaningless',
];
$kindDescription = <<<MAARKDOWN
- new: 新增的特性、功能、类、方法或 API
- enhanced: 对已存在的功能的加强或优化
- fixed: BUG 修复或对于某些故障的缓解措施
- removed: 移除或废弃的特性、功能、类、方法或 API
- meaningless: 没必要在 CHANGELOG 中提及的，如代码格式化、修正注释中的错别字、对于CI的调整、更新stub信息等
MAARKDOWN;

$addUrl = filter_var(getenv('ADD_URL'), FILTER_VALIDATE_BOOL);
$gptPlatform = getenv('GPT_PLATFORM') ?? throw new RuntimeException('Please set GPT_PLATFORM');
if ($gptPlatform !== 'azure') {
    throw new RuntimeException(sprintf('Only azure is supported for now, but got \'%s\'', $gptPlatform));
}
$gptBaseUrl = getenv('GPT_BASE_URL') ?? throw new RuntimeException('Please set GPT_BASE_URL');
$gptKey = getenv('GPT_KEY') ?? throw new RuntimeException('Please set GPT_KEY');
$noCache = filter_var(getenv('NO_CACHE'), FILTER_VALIDATE_BOOL);
$debug = filter_var(getenv('DEBUG'), FILTER_VALIDATE_BOOL);

$listPrefixMatcher = static function (string $kind): string {
    return match ($kind) {
        'new', 'enhanced' => '+',
        'removed' => '-',
        default => '*',
    };
};

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

$commits = array_map(static function (string $commit) use ($workspace, $authorMap) {
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
    $diff = shell_exec(sprintf('cd %s && git show --pretty="" --patch %s', $workspace, $commit['hash']));
    if (strlen($diff) > 2000) {
        $diff = substr($diff, 0, 2000) . "\n...";
    }
    $commit['diff'] = $diff;
    return $commit;
}, explode("\n\n", $gitLog));

$gptComplete = static function (string $message) use ($gptBaseUrl, $gptKey): string {
    $options = [
        'http' => [
            'method' => 'POST',
            'header' => [
                'Content-type: application/json',
                'accept: application/json, text/plain, */*',
                'api-key: ' . $gptKey,
            ],
            'content' => json_encode([
                'messages' => [
                    [
                        'role' => 'system',
                        'content' => 'You are an expert in git and version management, you will help me with the classification of git commits and the preparation of CHANGELOG writing'
                    ],
                    [
                        'role' => 'user',
                        'content' => $message,
                    ],
                ],
                'stream' => false,
                'max_tokens' => 3000,
                'temperature' => 0,
                'top_p' => 0.5,
                'presence_penalty' => 0.4,
                'frequency_penalty' => 0.4
            ])
        ]
    ];
    $context = stream_context_create($options);
    $response = file_get_contents($gptBaseUrl, false, $context);
    if ($response === false) {
        throw new RuntimeException('Failed to get GPT response');
    }
    return json_decode($response, true)['choices'][0]['message']['content'] ?? throw new RuntimeException('Failed to parse GPT response');
};

$changes = [];
foreach ($kinds as $kind) {
    $changes[$kind] = [];
}
// 将 commits 每三个分为一组
$commitGroups = array_chunk($commits, 5);
foreach ($commitGroups as $commitGroup) {
    $wr = new WaitReference();
    foreach ($commitGroup as $commit) {
        Coroutine::run(static function () use ($commit, &$changes, $listPrefixMatcher, $kindDescription, $addUrl, $gptComplete, $noCache, $debug, $wr): void {
            ['message' => $message, 'hash' => $hash, 'author' => $author, 'diff' => $diff] = $commit;
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
                return;
            }
            if (str_contains($messageLC, 'fix cs') ||
                str_contains($messageLC, 'code format')) {
                return;
            }
            if (str_starts_with($messageLC, 'sync deps')) {
                $kind = 'deps';
            } else {
                $gptCompletedKindCacheDir = sys_get_temp_dir() . '/swow-gen-changelog/gpt-completion-cache';
                if (!$noCache && is_file($gptCompletedKindCacheDir . '/' . $hash)) {
                    $replyString = file_get_contents($gptCompletedKindCacheDir . '/' . $hash);
                } else {
                    $question = <<<TEXT
commit message:
```
{$message}
```
code diff:
```
{$diff}
```
以上是 Git 提交的信息，提交分为以下四种类型：
```
{$kindDescription}
```
reply format:
```JSON
{"thinking_logic": "...", "kind": "..."}
```
请按照上述JSON格式回复，即在`thinking_logic`字段中先解释你对提交进行分类的逻辑（不要超过100个字），再通过`kind`字段告诉我这个提交归属于哪个分类，请不要在回答前后添加多余内容使得JSON解析失败。
TEXT;
                    $replyString = $gptComplete($question);
                    if (!is_dir($gptCompletedKindCacheDir)) {
                        mkdir($gptCompletedKindCacheDir, 0777, true);
                    }
                    file_put_contents($gptCompletedKindCacheDir . '/' . $hash, $replyString);
                }
                $reply = @json_decode($replyString, true);
                if (!$reply || !isset($reply['kind'])) {
                    if ($debug) {
                        var_dump($replyString);
                    }
                    goto _classification_downgrade;
                }
                if ($debug) {
                    $reply = [
                        'message' => $message,
                        ...$reply
                    ];
                    var_dump($reply);
                }
                $kind = $reply['kind'];
                if (str_contains($kind, 'new')) {
                    $kind = 'new';
                } elseif (str_contains($kind, 'enhanced')) {
                    $kind = 'enhanced';
                } elseif (str_contains($kind, 'fixed')) {
                    $kind = 'fixed';
                } elseif (str_contains($kind, 'removed')) {
                    $kind = 'removed';
                }  elseif (str_contains($kind, 'meaningless')) {
                    $kind = 'meaningless';
                } else {
                    _classification_downgrade:
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
            }
            $changes[$kind][] = sprintf(
                '%s %s (%s) (%s)',
                $listPrefixMatcher($kind),
                $message,
                $link,
                $addUrl ?
                    sprintf('[@%s](https://github.com/%s)', $author, $author) :
                    sprintf('@%s', $author),
            );
        });
    }
    $wr::wait($wr);
}
if ($debug) {
    var_dump($changes);
}

$date = date('Y-m-d');
echo <<<MARKDOWN
# [v{$versionId}](https://github.com/swow/swow/releases/tag/v{$versionId})

> release-date: {$date}

> 请随便写点什么有的没的。
>
> Please input a nonsense.


MARKDOWN;
foreach ($kinds as $kind) {
    if (!empty($changes[$kind])) {
        continue;
    }
    $changes[$kind] = ["{$listPrefixMatcher($kind)} Nothing {$kind}"];
}
$changes = array_map(static function (string|array $group): string {
    if (is_array($group)) {
        $group = implode("\n", $group);
    }
    return $group;
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

----

## Dropped

### meaningless

{$changes['meaningless']}

### deps

{$changes['deps']}

MARKDOWN;
