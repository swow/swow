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

namespace Swow\Psr7\Client;

use Generator;
use InvalidArgumentException;
use Psr\Http\Message\RequestInterface;
use Psr\Http\Message\ResponseInterface;
use Psr\Http\Message\UriInterface;
use Swow\Psr7\Psr7;
use Throwable;

use function array_change_key_case;
use function array_key_exists;
use function base64_encode;
use function filter_var;
use function http_build_query;
use function is_array;
use function is_int;
use function is_string;
use function ord;
use function pack;
use function preg_match;
use function strlen;
use function strpos;
use function substr;
use function strtolower;
use function strtoupper;

use const CASE_LOWER;
use const FILTER_FLAG_IPV4;
use const FILTER_FLAG_IPV6;
use const FILTER_VALIDATE_IP;
use const JSON_THROW_ON_ERROR;
use const JSON_UNESCAPED_SLASHES;
use const JSON_UNESCAPED_UNICODE;

class MagicClient implements ClientPlusInterface
{
    /**
     * @var ?array{
     *     type: string,
     *     host: string,
     *     port: int,
     *     username: ?string,
     *     password: ?string,
     *     remote_dns: bool
     * }
     */
    protected ?array $defaultProxy = null;

    /** @var array<string, mixed> */
    protected array $defaultTlsOptions = [];

    protected Client $client;

    protected ?int $connectTimeout = null;

    protected ?int $recvMessageTimeout = null;

    protected bool $streamingChunkedResponse = false;

    protected ?string $connectedScheme = null;

    protected ?string $connectedHost = null;

    protected ?int $connectedPort = null;

    protected ?string $connectedProxySignature = null;

    public function __construct(?Client $client = null)
    {
        $this->client = $client ?? new Client();
    }

    public function getClient(): Client
    {
        return $this->client;
    }

    public function setConnectTimeout(?int $timeout): static
    {
        $this->connectTimeout = $timeout;
        return $this;
    }

    public function setRecvMessageTimeout(int $timeout): static
    {
        $this->recvMessageTimeout = $timeout;
        $this->client->setRecvMessageTimeout($timeout);
        return $this;
    }

    public function setStreamingChunkedResponse(bool $enable): static
    {
        $this->streamingChunkedResponse = $enable;
        $this->client->setStreamingChunkedResponse($enable);
        return $this;
    }

    /**
     * @param ?array<string, mixed> $proxy
     */
    public function setProxy(?array $proxy): static
    {
        $this->defaultProxy = $this->normalizeProxyConfig($proxy);
        return $this;
    }

    public function clearProxy(): static
    {
        $this->defaultProxy = null;
        return $this;
    }

    /**
     * @param array<string, mixed> $tlsOptions
     */
    public function setTlsOptions(array $tlsOptions): static
    {
        $this->defaultTlsOptions = $this->normalizeTlsOptions($tlsOptions);
        return $this;
    }

    public function sendRequest(RequestInterface $request): ResponseInterface
    {
        $proxy = $this->defaultProxy;
        $request = $this->applyTransportRequest($request, $proxy);
        $this->ensureConnectedForUri($request->getUri(), $request, $proxy, $this->defaultTlsOptions);
        return $this->client->sendRequest($request);
    }

    /**
     * @return Generator<int, \Swow\Psr7\Message\EventStreamEvent>
     */
    public function sendEventStreamRequest(RequestInterface $request, ?int $timeout = null, int $readSize = 8192): Generator
    {
        $proxy = $this->defaultProxy;
        $request = $this->applyTransportRequest($request, $proxy);
        $this->ensureConnectedForUri($request->getUri(), $request, $proxy, $this->defaultTlsOptions);
        return $this->client->sendEventStreamRequest($request, $timeout, $readSize);
    }

    /**
     * @param array<string, mixed> $options
     */
    public function request(string $method, string $url, array $options = []): ResponseInterface
    {
        $proxy = $this->resolveProxyConfig($options);
        $tlsOptions = $this->resolveTlsOptions($options);
        $request = $this->buildRequest(strtoupper($method), $url, $options);
        $request = $this->applyTransportRequest($request, $proxy);
        $timeout = $this->normalizeOptionalTimeout($options['timeout'] ?? null);
        $this->ensureConnectedForUri($request->getUri(), $request, $proxy, $tlsOptions);
        return $this->client->sendRequest($request, $timeout);
    }

    /**
     * @param array<string, mixed> $options
     */
    public function get(string $url, array $options = []): ResponseInterface
    {
        return $this->request('GET', $url, $options);
    }

    /**
     * @param array<string, mixed> $options
     */
    public function post(string $url, array $options = []): ResponseInterface
    {
        return $this->request('POST', $url, $options);
    }

    /**
     * @param array<string, mixed> $options
     * @return Generator<int, \Swow\Psr7\Message\EventStreamEvent>
     */
    public function stream(string $url, array $options = []): Generator
    {
        $proxy = $this->resolveProxyConfig($options);
        $tlsOptions = $this->resolveTlsOptions($options);
        $method = strtoupper((string) ($options['method'] ?? 'GET'));
        $request = $this->buildRequest($method, $url, $options);
        $request = $this->applyTransportRequest($request, $proxy);
        $this->setStreamingChunkedResponse((bool) ($options['streaming_chunked'] ?? true));
        $timeout = $this->normalizeOptionalTimeout($options['timeout'] ?? null);
        $readSize = $this->normalizeReadSize($options['read_size'] ?? 8192);
        $this->ensureConnectedForUri($request->getUri(), $request, $proxy, $tlsOptions);
        return $this->client->sendEventStreamRequest($request, $timeout, $readSize);
    }

    protected function closeUnderlyingClientSilently(): void
    {
        try {
            $this->client->close();
        } catch (Throwable) {
            // 忽略 close 过程中的异常，后续会走一次全新的连接链路。
        }
    }

    protected function recreateClient(): void
    {
        $this->closeUnderlyingClientSilently();
        $this->client = new Client();
        $this->client->setStreamingChunkedResponse($this->streamingChunkedResponse);
        if ($this->recvMessageTimeout !== null) {
            $this->client->setRecvMessageTimeout($this->recvMessageTimeout);
        }
        $this->connectedScheme = null;
        $this->connectedHost = null;
        $this->connectedPort = null;
        $this->connectedProxySignature = null;
    }

    /**
     * @param ?array{
     *     type: string,
     *     host: string,
     *     port: int,
     *     username: ?string,
     *     password: ?string,
     *     remote_dns: bool
     * } $proxy
     * @param array<string, mixed> $tlsOptions
     */
    protected function ensureConnectedForUri(UriInterface $uri, RequestInterface $request, ?array $proxy, array $tlsOptions): void
    {
        $host = strtolower($uri->getHost());
        if ($host === '') {
            throw new ClientRequestException($request, 'MagicClient requires an absolute URI with host');
        }
        $scheme = strtolower($uri->getScheme());
        if ($scheme === '') {
            $scheme = 'http';
        }
        if ($scheme !== 'http' && $scheme !== 'https') {
            throw new ClientRequestException($request, "MagicClient only supports http/https scheme, got: {$scheme}");
        }
        $port = $uri->getPort() ?? ($scheme === 'https' ? 443 : 80);
        $proxySignature = $this->buildProxySignature($proxy);
        $endpointChanged = $this->connectedScheme !== $scheme
            || $this->connectedHost !== $host
            || $this->connectedPort !== $port
            || $this->connectedProxySignature !== $proxySignature;
        if (!$endpointChanged) {
            return;
        }

        $this->recreateClient();
        if ($proxy === null) {
            $this->client->connect($host, $port, $this->connectTimeout);
            if ($scheme === 'https') {
                $this->client->enableCrypto($this->buildCryptoOptions($host, $tlsOptions));
            }
        } else {
            $this->client->connect($proxy['host'], $proxy['port'], $this->connectTimeout);
            if ($proxy['type'] === 'http') {
                if ($scheme === 'https') {
                    $this->connectViaHttpProxyTunnel($request, $proxy, $host, $port);
                    $this->client->enableCrypto($this->buildCryptoOptions($host, $tlsOptions));
                }
            } else {
                $this->connectViaSocks5Proxy($request, $proxy, $host, $port);
                if ($scheme === 'https') {
                    $this->client->enableCrypto($this->buildCryptoOptions($host, $tlsOptions));
                }
            }
        }
        $this->connectedScheme = $scheme;
        $this->connectedHost = $host;
        $this->connectedPort = $port;
        $this->connectedProxySignature = $proxySignature;
    }

    /**
     * @param array{
     *     type: string,
     *     host: string,
     *     port: int,
     *     username: ?string,
     *     password: ?string,
     *     remote_dns: bool
     * } $proxy
     */
    protected function connectViaHttpProxyTunnel(RequestInterface $request, array $proxy, string $targetHost, int $targetPort): void
    {
        $targetEndpoint = "{$targetHost}:{$targetPort}";
        $connectRequest = "CONNECT {$targetEndpoint} HTTP/1.1\r\n" .
            "Host: {$targetEndpoint}\r\n" .
            "Proxy-Connection: keep-alive\r\n";
        $proxyAuthorization = $this->buildProxyAuthorization($proxy);
        if ($proxyAuthorization !== null) {
            $connectRequest .= "Proxy-Authorization: {$proxyAuthorization}\r\n";
        }
        $connectRequest .= "\r\n";
        $this->client->send($connectRequest);

        $rawHeaders = '';
        while (($headerEndPos = strpos($rawHeaders, "\r\n\r\n")) === false) {
            $chunk = $this->client->recvString(1024, $this->connectTimeout);
            if ($chunk === '') {
                throw new ClientRequestException($request, 'HTTP proxy CONNECT failed: proxy closed before response headers completed');
            }
            $rawHeaders .= $chunk;
            if (strlen($rawHeaders) > 16 * 1024) {
                throw new ClientRequestException($request, 'HTTP proxy CONNECT failed: response headers too large');
            }
        }
        $statusLineEnd = strpos($rawHeaders, "\r\n");
        if ($statusLineEnd === false) {
            throw new ClientRequestException($request, 'HTTP proxy CONNECT failed: malformed status line');
        }
        $statusLine = substr($rawHeaders, 0, $statusLineEnd);
        if (!preg_match('/^HTTP\/\d+\.\d+\s+(\d{3})(?:\s+(.*))?$/', $statusLine, $matches)) {
            throw new ClientRequestException($request, "HTTP proxy CONNECT failed: invalid status line {$statusLine}");
        }
        $statusCode = (int) $matches[1];
        $reasonPhrase = (string) ($matches[2] ?? '');
        if ($statusCode !== 200) {
            throw new ClientRequestException(
                $request,
                "HTTP proxy CONNECT failed: status={$statusCode} reason={$reasonPhrase}"
            );
        }
    }

    /**
     * @param array{
     *     type: string,
     *     host: string,
     *     port: int,
     *     username: ?string,
     *     password: ?string,
     *     remote_dns: bool
     * } $proxy
     */
    protected function connectViaSocks5Proxy(RequestInterface $request, array $proxy, string $targetHost, int $targetPort): void
    {
        $proxyAuthorization = $this->buildProxyAuthorization($proxy);
        $methods = $proxyAuthorization === null ? "\x00" : "\x00\x02";
        $this->client->send("\x05" . pack('C', strlen($methods)) . $methods);
        $methodReply = $this->client->readString(2, $this->connectTimeout);
        if (ord($methodReply[0]) !== 0x05) {
            throw new ClientRequestException($request, 'SOCKS5 handshake failed: invalid version in method reply');
        }
        $selectedMethod = ord($methodReply[1]);
        if ($selectedMethod === 0xFF) {
            throw new ClientRequestException($request, 'SOCKS5 handshake failed: no acceptable auth method');
        }
        if ($selectedMethod !== 0x00 && $selectedMethod !== 0x02) {
            throw new ClientRequestException($request, "SOCKS5 handshake failed: unsupported auth method={$selectedMethod}");
        }
        if ($selectedMethod === 0x02) {
            $username = $proxy['username'] ?? '';
            $password = $proxy['password'] ?? '';
            $this->client->send(
                "\x01" .
                pack('C', strlen($username)) . $username .
                pack('C', strlen($password)) . $password
            );
            $authReply = $this->client->readString(2, $this->connectTimeout);
            if (ord($authReply[1]) !== 0x00) {
                throw new ClientRequestException($request, 'SOCKS5 auth failed: username/password rejected');
            }
        }

        $addressPayload = $this->buildSocks5AddressPayload($targetHost, $proxy['remote_dns']);
        $this->client->send("\x05\x01\x00" . $addressPayload . pack('n', $targetPort));
        $connectHead = $this->client->readString(4, $this->connectTimeout);
        if (ord($connectHead[0]) !== 0x05) {
            throw new ClientRequestException($request, 'SOCKS5 CONNECT failed: invalid version in response');
        }
        $replyCode = ord($connectHead[1]);
        if ($replyCode !== 0x00) {
            throw new ClientRequestException($request, "SOCKS5 CONNECT failed: reply_code={$replyCode}");
        }
        $addressType = ord($connectHead[3]);
        if ($addressType === 0x01) {
            $this->client->readString(4, $this->connectTimeout);
        } elseif ($addressType === 0x03) {
            $length = ord($this->client->readString(1, $this->connectTimeout));
            if ($length > 0) {
                $this->client->readString($length, $this->connectTimeout);
            }
        } elseif ($addressType === 0x04) {
            $this->client->readString(16, $this->connectTimeout);
        } else {
            throw new ClientRequestException($request, "SOCKS5 CONNECT failed: invalid address type={$addressType}");
        }
        $this->client->readString(2, $this->connectTimeout);
    }

    protected function buildSocks5AddressPayload(string $host, bool $remoteDns): string
    {
        $ipv4 = filter_var($host, FILTER_VALIDATE_IP, FILTER_FLAG_IPV4);
        if ($ipv4 !== false) {
            return "\x01" . pack('C4', ...array_map('intval', explode('.', $host)));
        }
        $ipv6 = filter_var($host, FILTER_VALIDATE_IP, FILTER_FLAG_IPV6);
        if ($ipv6 !== false) {
            return "\x04" . (string) inet_pton($host);
        }
        if (!$remoteDns) {
            throw new InvalidArgumentException('SOCKS5 with remote_dns=false requires an IP host');
        }
        if (strlen($host) > 255) {
            throw new InvalidArgumentException('SOCKS5 host length must be <= 255 bytes');
        }
        return "\x03" . pack('C', strlen($host)) . $host;
    }

    protected function buildProxySignature(?array $proxy): string
    {
        if ($proxy === null) {
            return 'direct';
        }
        $user = $proxy['username'] ?? '';
        $password = $proxy['password'] ?? '';
        $remoteDns = $proxy['remote_dns'] ? '1' : '0';
        return "{$proxy['type']}://{$user}:{$password}@{$proxy['host']}:{$proxy['port']}?remote_dns={$remoteDns}";
    }

    /**
     * @param ?array{
     *     type: string,
     *     host: string,
     *     port: int,
     *     username: ?string,
     *     password: ?string,
     *     remote_dns: bool
     * } $proxy
     */
    protected function applyTransportRequest(RequestInterface $request, ?array $proxy): RequestInterface
    {
        if ($proxy === null) {
            return $request;
        }
        $uri = $request->getUri();
        if ($proxy['type'] === 'http' && strtolower($uri->getScheme()) === 'http') {
            $request = $request->withRequestTarget((string) $uri);
            $proxyAuthorization = $this->buildProxyAuthorization($proxy);
            if ($proxyAuthorization !== null) {
                $request = $request->withHeader('Proxy-Authorization', $proxyAuthorization);
            }
        }
        return $request;
    }

    /**
     * @param array<string, mixed> $options
     * @return ?array{
     *     type: string,
     *     host: string,
     *     port: int,
     *     username: ?string,
     *     password: ?string,
     *     remote_dns: bool
     * }
     */
    protected function resolveProxyConfig(array $options): ?array
    {
        if (array_key_exists('proxy', $options)) {
            return $this->normalizeProxyConfig($options['proxy']);
        }
        return $this->defaultProxy;
    }

    /**
     * @param array<string, mixed> $options
     * @return array<string, mixed>
     */
    protected function resolveTlsOptions(array $options): array
    {
        if (array_key_exists('tls', $options)) {
            return $this->normalizeTlsOptions($options['tls']);
        }
        return $this->defaultTlsOptions;
    }

    /**
     * @param mixed $proxy
     * @return ?array{
     *     type: string,
     *     host: string,
     *     port: int,
     *     username: ?string,
     *     password: ?string,
     *     remote_dns: bool
     * }
     */
    protected function normalizeProxyConfig(mixed $proxy): ?array
    {
        if ($proxy === null) {
            return null;
        }
        if (!is_array($proxy)) {
            throw new InvalidArgumentException('Option "proxy" must be null or an array');
        }
        $type = strtolower((string) ($proxy['type'] ?? ''));
        if ($type === 'socks5h') {
            $type = 'socks5';
            $proxy['remote_dns'] = true;
        }
        if ($type !== 'http' && $type !== 'socks5') {
            throw new InvalidArgumentException('Option "proxy.type" must be one of: http, socks5, socks5h');
        }
        $host = strtolower((string) ($proxy['host'] ?? ''));
        if ($host === '') {
            throw new InvalidArgumentException('Option "proxy.host" must be a non-empty string');
        }
        $port = $proxy['port'] ?? null;
        if (!is_int($port) || $port < 1 || $port > 65535) {
            throw new InvalidArgumentException('Option "proxy.port" must be an int between 1 and 65535');
        }
        $username = $proxy['username'] ?? null;
        $password = $proxy['password'] ?? null;
        if (($username === null) xor ($password === null)) {
            throw new InvalidArgumentException('Option "proxy.username" and "proxy.password" must be set together');
        }
        if ($username !== null && !is_string($username)) {
            throw new InvalidArgumentException('Option "proxy.username" must be a string');
        }
        if ($password !== null && !is_string($password)) {
            throw new InvalidArgumentException('Option "proxy.password" must be a string');
        }
        if ($username !== null && (strlen($username) > 255 || strlen($password ?? '') > 255)) {
            throw new InvalidArgumentException('SOCKS5 username/password length must be <= 255 bytes');
        }
        $remoteDns = (bool) ($proxy['remote_dns'] ?? true);
        return [
            'type' => $type,
            'host' => $host,
            'port' => $port,
            'username' => $username,
            'password' => $password,
            'remote_dns' => $remoteDns,
        ];
    }

    /**
     * @param mixed $tls
     * @return array<string, mixed>
     */
    protected function normalizeTlsOptions(mixed $tls): array
    {
        if ($tls === null) {
            return [];
        }
        if (!is_array($tls)) {
            throw new InvalidArgumentException('Option "tls" must be an array');
        }
        return $tls;
    }

    /**
     * @param array{
     *     type: string,
     *     host: string,
     *     port: int,
     *     username: ?string,
     *     password: ?string,
     *     remote_dns: bool
     * } $proxy
     */
    protected function buildProxyAuthorization(array $proxy): ?string
    {
        if ($proxy['username'] === null || $proxy['password'] === null) {
            return null;
        }
        return 'Basic ' . base64_encode($proxy['username'] . ':' . $proxy['password']);
    }

    /**
     * @param array<string, mixed> $tlsOptions
     * @return array<string, mixed>
     */
    protected function buildCryptoOptions(string $host, array $tlsOptions): array
    {
        return ['peer_name' => $host] + $tlsOptions;
    }

    /**
     * @param array<string, mixed> $options
     */
    protected function buildRequest(string $method, string $url, array $options): RequestInterface
    {
        $uri = Psr7::createUriFromString($url);
        $query = $options['query'] ?? null;
        if (is_array($query) && $query !== []) {
            $queryString = http_build_query($query);
            $baseQuery = $uri->getQuery();
            if ($baseQuery !== '' && $queryString !== '') {
                $uri = $uri->withQuery($baseQuery . '&' . $queryString);
            } else {
                $uri = $uri->withQuery($queryString);
            }
        }

        $headers = $this->normalizeHeaders($options['headers'] ?? []);
        $hasJson = array_key_exists('json', $options);
        $hasBody = array_key_exists('body', $options);
        if ($hasJson && $hasBody) {
            throw new InvalidArgumentException('Options "json" and "body" are mutually exclusive');
        }
        $body = $options['body'] ?? null;
        if ($hasJson) {
            $jsonPayload = $options['json'];
            $body = is_string($jsonPayload)
                ? $jsonPayload
                : (string) json_encode($jsonPayload, JSON_THROW_ON_ERROR | JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
            if (!array_key_exists('content-type', $headers)) {
                $headers['Content-Type'] = 'application/json';
            }
        }

        return Psr7::createRequest(
            method: $method,
            uri: $uri,
            headers: $headers,
            body: $body,
        );
    }

    /**
     * @param mixed $timeout
     */
    protected function normalizeOptionalTimeout(mixed $timeout): ?int
    {
        if ($timeout === null) {
            return null;
        }
        if (!is_int($timeout) || $timeout < 0) {
            throw new InvalidArgumentException('Option "timeout" must be null or a non-negative int');
        }
        return $timeout;
    }

    /**
     * @param mixed $readSize
     */
    protected function normalizeReadSize(mixed $readSize): int
    {
        if (!is_int($readSize) || $readSize <= 0) {
            throw new InvalidArgumentException('Option "read_size" must be a positive int');
        }
        return $readSize;
    }

    /**
     * @param mixed $headers
     * @return array<string, mixed>
     */
    protected function normalizeHeaders(mixed $headers): array
    {
        if (!is_array($headers)) {
            throw new InvalidArgumentException('Option "headers" must be an array');
        }
        $lowerHeaders = array_change_key_case($headers, CASE_LOWER);
        if (array_key_exists('host', $lowerHeaders)) {
            unset($headers['host'], $headers['Host'], $headers['HOST']);
        }
        return $headers;
    }
}
