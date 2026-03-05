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
    'messages' => [
        ['role' => 'system', 'content' => '你是一个知识渊博的 AI 专家'],
        ['role' => 'user', 'content' => '请你写一篇主题为「Agentic 与 Workflow 范式之争」的文章，不得少于 5000 字'],
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

$receivedChars = 0;
$done = false;
foreach (Psr7::readEventStream($response->getBody()) as $event) {
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
