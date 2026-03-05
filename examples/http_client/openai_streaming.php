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

use Swow\Psr7\Client\Client;
use Swow\Psr7\Psr7;

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

$platform = strtolower(requireEnv('GPT_PLATFORM'));
$baseUrl = requireEnv('GPT_BASE_URL');
$apiKey = requireEnv('GPT_KEY');

$url = parse_url($baseUrl);
if ($url === false || !isset($url['host'])) {
    throw new \RuntimeException('Invalid GPT_BASE_URL');
}

$scheme = strtolower((string) ($url['scheme'] ?? 'https'));
$host = (string) $url['host'];
$port = (int) ($url['port'] ?? ($scheme === 'https' ? 443 : 80));
$path = (string) ($url['path'] ?? '/');
if ($path === '') {
    $path = '/';
}
if (isset($url['query']) && $url['query'] !== '') {
    $path .= '?' . $url['query'];
}

$hostHeader = $host;
if (!(($scheme === 'https' && $port === 443) || ($scheme === 'http' && $port === 80))) {
    $hostHeader .= ':' . $port;
}

$headers = [
    'Host' => $hostHeader,
    'Content-Type' => 'application/json',
    'Accept' => 'text/event-stream',
    'Connection' => 'keep-alive',
];

if ($platform === 'azure') {
    $headers['api-key'] = $apiKey;
} else {
    $headers['Authorization'] = 'Bearer ' . $apiKey;
}

$requestBody = [
    'stream' => true,
    'max_completion_tokens' => 12000,
    'messages' => [
        ['role' => 'system', 'content' => '你是中文技术写作助手，输出要结构清晰、内容完整。'],
        ['role' => 'user', 'content' => '请写一篇中文技术长文，主题是「基于协程的高并发 HTTP 客户端设计实践」，正文必须不少于 6000 字。文章需包含：背景问题、架构设计、关键流程、错误处理、性能优化、可维护性、常见坑与排障、总结。请直接输出正文，不要解释任务，不要提问，不要引导“是否继续”。'],
    ],
];

// 非 Azure 平台通常要求显式 model；支持通过 GPT_MODEL 覆盖。
if ($platform !== 'azure') {
    $requestBody['model'] = trim((string) getenv('GPT_MODEL')) ?: 'gpt-4o-mini';
}

$request = Psr7::createRequest(
    method: 'POST',
    uri: $path,
    headers: $headers,
    body: json_encode($requestBody, JSON_THROW_ON_ERROR | JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES),
);

$client = (new Client())
    ->setStreamingChunkedResponse(true)
    ->connect($host, $port);

if ($scheme === 'https') {
    $client->enableCrypto([
        'peer_name' => $host,
    ]);
}

$response = $client->sendRequest($request);

echo 'HTTP ' . $response->getStatusCode() . ' ' . $response->getReasonPhrase() . PHP_EOL;
echo str_repeat('-', 60) . PHP_EOL;

$body = $response->getBody();
$lineBuffer = '';
$receivedChars = 0;
while (!$body->eof()) {
    $chunk = $body->read(8192);
    if ($chunk === '') {
        continue;
    }
    $lineBuffer .= $chunk;

    while (($pos = strpos($lineBuffer, "\n")) !== false) {
        $line = trim(substr($lineBuffer, 0, $pos), "\r");
        $lineBuffer = (string) substr($lineBuffer, $pos + 1);

        if ($line === '' || !str_starts_with($line, 'data:')) {
            continue;
        }
        $payload = trim((string) substr($line, 5));
        if ($payload === '[DONE]') {
            echo PHP_EOL . '[DONE]' . PHP_EOL;
            break 2;
        }

        $json = json_decode($payload, true);
        if (!is_array($json)) {
            continue;
        }

        $delta = $json['choices'][0]['delta']['content'] ?? null;
        if (is_string($delta) && $delta !== '') {
            echo $delta;
            // 立即刷新到终端，长文本时更容易看到流式效果。
            if (ob_get_level() > 0) {
                @ob_flush();
            }
            @flush();
            $receivedChars += preg_match_all('/./u', $delta);
        }
    }
}

echo PHP_EOL;
echo '[RECEIVED_CHARS] ' . $receivedChars . PHP_EOL;
