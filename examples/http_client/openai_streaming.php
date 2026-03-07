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

use Swow\Psr7\Client\MagicClient;

require __DIR__ . '/../autoload.php';

/**
 * 从环境变量读取配置；为空时直接失败，避免发出错误请求。
 */
function requireEnv(string $name): string
{
    $value = trim((string) getenv($name));
    if ($value === '') {
        throw new \RuntimeException(sprintf('Missing required env: %s', $name));
    }
    return $value;
}

/**
 * @param list<string> $names
 */
function getEnvOrEmpty(array $names): string
{
    foreach ($names as $name) {
        $value = trim((string) getenv($name));
        if ($value !== '') {
            return $value;
        }
    }
    return '';
}

/**
 * @return ?array{
 *     type: string,
 *     host: string,
 *     port: int,
 *     username: ?string,
 *     password: ?string,
 *     remote_dns?: bool
 * }
 */
function parseProxyFromEnv(): ?array
{
    $allProxy = getEnvOrEmpty(['all_proxy', 'ALL_PROXY']);
    if ($allProxy !== '') {
        $parts = parse_url($allProxy);
        if ($parts !== false && isset($parts['host'])) {
            $scheme = strtolower((string) ($parts['scheme'] ?? 'socks5'));
            if ($scheme === 'socks5' || $scheme === 'socks5h') {
                return [
                    'type' => $scheme,
                    'host' => (string) $parts['host'],
                    'port' => (int) ($parts['port'] ?? 1080),
                    'username' => isset($parts['user']) ? (string) $parts['user'] : null,
                    'password' => isset($parts['pass']) ? (string) $parts['pass'] : null,
                    'remote_dns' => $scheme === 'socks5h',
                ];
            }
        }
    }

    $httpProxy = getEnvOrEmpty(['https_proxy', 'HTTPS_PROXY', 'http_proxy', 'HTTP_PROXY']);
    if ($httpProxy === '') {
        return null;
    }
    $parts = parse_url($httpProxy);
    if ($parts === false || !isset($parts['host'])) {
        return null;
    }
    return [
        'type' => 'http',
        'host' => (string) $parts['host'],
        'port' => (int) ($parts['port'] ?? 80),
        'username' => isset($parts['user']) ? (string) $parts['user'] : null,
        'password' => isset($parts['pass']) ? (string) $parts['pass'] : null,
    ];
}

$platform = strtolower(requireEnv('GPT_PLATFORM'));
$baseUrl = requireEnv('GPT_BASE_URL');
$apiKey = requireEnv('GPT_KEY');

$headers = [
    'Accept' => 'text/event-stream',
];

if ($platform === 'azure') {
    $headers['api-key'] = $apiKey;
} else {
    $headers['Authorization'] = 'Bearer ' . $apiKey;
}

$requestBody = [
    'stream' => true,
    'messages' => [
        ['role' => 'system', 'content' => '你是一个知识渊博的 AI 专家'],
        ['role' => 'user', 'content' => '请你写一篇主题为「Agentic 与 Workflow 范式之争」的文章，不得少于 5000 字'],
    ],
];

// 非 Azure 平台通常要求显式 model；支持通过 GPT_MODEL 覆盖。
if ($platform !== 'azure') {
    $requestBody['model'] = trim((string) getenv('GPT_MODEL')) ?: 'gpt-4o-mini';
}

$proxy = parseProxyFromEnv();
$client = (new MagicClient())
    ->setConnectTimeout(10 * 1000 * 1000)
    ->setRecvMessageTimeout(60 * 1000 * 1000)
    ->setStreamingChunkedResponse(true);
if ($proxy !== null) {
    $client->setProxy($proxy);
}

$receivedChars = 0;
$done = false;
foreach ($client->stream($baseUrl, [
    'method' => 'POST',
    'headers' => $headers,
    'json' => $requestBody,
    'streaming_chunked' => true,
]) as $event) {
    $payload = trim($event->data);
    if ($payload === '') {
        continue;
    }

    if ($payload === '[DONE]') {
        $done = true;
        break;
    }

    $json = json_decode($payload, true);
    if (!is_array($json)) {
        continue;
    }

    $delta = $json['choices'][0]['delta']['content'] ?? null;
    if (is_string($delta) && $delta !== '') {
        echo $delta;
        $receivedChars += preg_match_all('/./u', $delta);
    }
}

echo PHP_EOL;
if ($done) {
    echo '[DONE]' . PHP_EOL;
}
echo '[RECEIVED_CHARS] ' . $receivedChars . PHP_EOL;
