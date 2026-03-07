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
 * @param list<string> $names
 */
function getProxyEnvOrDefault(array $names, string $default): string
{
    foreach ($names as $name) {
        $value = trim((string) getenv($name));
        if ($value !== '') {
            return $value;
        }
    }
    return $default;
}

/**
 * @return array{
 *     type: string,
 *     host: string,
 *     port: int,
 *     username: ?string,
 *     password: ?string
 * }
 */
function parseHttpProxy(string $proxyUrl): array
{
    $parts = parse_url($proxyUrl);
    if ($parts === false || !isset($parts['host'])) {
        throw new RuntimeException("Invalid proxy url: {$proxyUrl}");
    }
    $scheme = strtolower((string) ($parts['scheme'] ?? 'http'));
    if ($scheme !== 'http') {
        throw new RuntimeException("HTTP proxy example requires http://..., got: {$proxyUrl}");
    }
    return [
        'type' => 'http',
        'host' => (string) $parts['host'],
        'port' => (int) ($parts['port'] ?? 80),
        'username' => isset($parts['user']) ? (string) $parts['user'] : null,
        'password' => isset($parts['pass']) ? (string) $parts['pass'] : null,
    ];
}

$proxy = parseHttpProxy(getProxyEnvOrDefault(
    ['https_proxy', 'HTTPS_PROXY', 'http_proxy', 'HTTP_PROXY'],
    'http://127.0.0.1:7890'
));
$client = (new MagicClient())
    ->setConnectTimeout(5 * 1000 * 1000)
    ->setRecvMessageTimeout(10 * 1000 * 1000);

$response = $client->request('GET', 'https://www.google.com/', [
    'proxy' => $proxy,
    'headers' => [
        'User-Agent' => 'swow-magic-client-example/1.0',
        'Accept' => 'text/html',
    ],
]);

$body = (string) $response->getBody();
echo 'HTTP ' . $response->getStatusCode() . ' ' . $response->getReasonPhrase() . PHP_EOL;
echo 'Body bytes: ' . strlen($body) . PHP_EOL;
echo 'Contains "google": ' . (str_contains(strtolower($body), 'google') ? 'yes' : 'no') . PHP_EOL;
echo str_repeat('-', 60) . PHP_EOL;
echo substr($body, 0, 300) . PHP_EOL;
