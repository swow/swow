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

use Exception;
use Generator;
use Psr\Http\Client\ClientExceptionInterface;
use Psr\Http\Message\RequestInterface;
use Psr\Http\Message\ResponseInterface;
use Swow\Coroutine;
use Swow\Errno;
use Swow\Http\Http;
use Swow\Http\Message\ResponseEntity;
use Swow\Http\Parser;
use Swow\Http\Parser as HttpParser;
use Swow\Http\Protocol\ProtocolTypeInterface;
use Swow\Http\Protocol\ProtocolTypeTrait;
use Swow\Http\Protocol\ReceiverTrait;
use Swow\Http\Status as HttpStatus;
use Swow\Psr7\Config\LimitationTrait;
use Swow\Psr7\Message\ClientPsr17FactoryTrait;
use Swow\Psr7\Message\Request;
use Swow\Psr7\Protocol\WebSocketTrait;
use Swow\Psr7\Psr7;
use Swow\Socket;
use Swow\SocketException;
use Swow\WebSocket\WebSocket;

use function base64_encode;
use function random_bytes;
use function strlen;

class Client implements ClientPlusInterface, ProtocolTypeInterface
{
    use ClientPsr17FactoryTrait;

    use LimitationTrait;

    use ProtocolTypeTrait;

    /**
     * @use ReceiverTrait<ResponseEntity>
     */
    use ReceiverTrait;

    use WebSocketTrait;

    public const TYPE_TCP = Socket::TYPE_TCP;
    public const TYPE_TCP4 = Socket::TYPE_TCP4;
    public const TYPE_TCP6 = Socket::TYPE_TCP6;

    public const DEFAULT_HTTP_PARSER_EVENTS =
        HttpParser::EVENT_STATUS |
        HttpParser::EVENT_HEADER_FIELD |
        HttpParser::EVENT_HEADER_VALUE |
        HttpParser::EVENT_HEADERS_COMPLETE |
        HttpParser::EVENT_CHUNK_HEADER |
        HttpParser::EVENT_CHUNK_COMPLETE |
        HttpParser::EVENT_BODY |
        HttpParser::EVENT_MESSAGE_COMPLETE;

    private Socket $socket;

    private int $socketType;

    protected string $host = '';

    public function __construct(int $type = self::TYPE_TCP)
    {
        $this->socketType = $type;
        $this->socket = new Socket($type);
        $this->__constructReceiver(Parser::TYPE_RESPONSE, static::DEFAULT_HTTP_PARSER_EVENTS);
        $this->__constructClientPsr17Factory();
    }

    public function getSocket(): Socket
    {
        return $this->socket;
    }

    public function getSocketType(): int
    {
        return $this->socketType;
    }

    public function close(): bool
    {
        return $this->socket->close();
    }

    public function __debugInfo(): array
    {
        return $this->socket->__debugInfo();
    }

    public function connect(string $name, int $port = 0, ?int $timeout = null): static
    {
        $this->host = $name;
        $this->getSocket()->connect($name, $port, $timeout);

        return $this;
    }

    public function recreate(?int $type = null): static
    {
        $type ??= $this->socketType;
        $this->socket->recreate($type);
        $this->socketType = $type;
        $this->host = '';
        $this->protocolType = self::PROTOCOL_TYPE_HTTP;
        $this->__constructReceiver(Parser::TYPE_RESPONSE, static::DEFAULT_HTTP_PARSER_EVENTS);

        return $this;
    }

    public function enableCrypto(?array $options = null): static
    {
        $this->getSocket()->enableCrypto($options);

        return $this;
    }

    public function isEstablished(): bool
    {
        return $this->getSocket()->isEstablished();
    }

    public function getReadTimeout(): int
    {
        return $this->getSocket()->getReadTimeout();
    }

    public function setTimeout(int $timeout): static
    {
        $this->getSocket()->setTimeout($timeout);

        return $this;
    }

    public function setConnectTimeout(int $timeout): static
    {
        $this->getSocket()->setConnectTimeout($timeout);

        return $this;
    }

    public function setHandshakeTimeout(int $timeout): static
    {
        $this->getSocket()->setHandshakeTimeout($timeout);

        return $this;
    }

    public function setReadTimeout(int $timeout): static
    {
        $this->getSocket()->setReadTimeout($timeout);

        return $this;
    }

    public function setWriteTimeout(int $timeout): static
    {
        $this->getSocket()->setWriteTimeout($timeout);

        return $this;
    }

    /** @param array<string, array<string>> $headers */
    public function sendPackedRequestAsync(
        string $method,
        string $uri,
        array $headers = [],
        string $body = '',
        string $protocolVersion = Request::DEFAULT_PROTOCOL_VERSION,
    ): static {
        $this->getSocket()->write([
            Http::packRequest(
                method: $method,
                uri: $uri,
                headers: $headers,
                protocolVersion: $protocolVersion
            ),
            $body,
        ]);

        return $this;
    }

    public function recvResponseEntity(?int $timeout = null): ResponseEntity
    {
        return $this->recvMessageEntity(timeout: $timeout);
    }

    public function sendRequest(RequestInterface $request, ?int $timeout = null): ResponseInterface
    {
        $this->receivingHeadResponse = strcasecmp($request->getMethod(), 'HEAD') === 0;
        try {
            $headers = $request->getHeaders();
            $body = (string) $request->getBody();
            // TODO: add standardize util function to do that?
            if (!$request->hasHeader('host')) {
                $headers['Host'] = $this->host;
            }
            $contentLength = strlen($body);
            if ($contentLength !== 0 && !$request->hasHeader('content-length')) {
                $headers['Content-Length'] = $contentLength;
            }
            if (!$request->hasHeader('connection')) {
                $headers['Connection'] = 'keep-alive';
            }

            $this->sendPackedRequestAsync(
                method: $request->getMethod(),
                uri: $request->getRequestTarget(),
                headers: $headers,
                body: $body,
                protocolVersion: $request->getProtocolVersion(),
            );
            $timeout ??= $this->recvMessageTimeout;
            $startTime = Coroutine::getMain()->getElapsed();
            do {
                $remainingTimeout = $timeout;
                if ($timeout >= 0) {
                    $elapsed = Coroutine::getMain()->getElapsed() - $startTime;
                    if ($elapsed > $timeout) {
                        throw new SocketException('Recv final HTTP response timeout', Errno::ETIMEDOUT);
                    }
                    $remainingTimeout -= $elapsed;
                }
                $responseEntity = $this->recvResponseEntity($remainingTimeout);
                // Informational responses precede the final response; 101 transfers protocol ownership.
            } while ($responseEntity->statusCode < 200 && $responseEntity->statusCode !== HttpStatus::SWITCHING_PROTOCOLS);

            return Psr7::createResponseFromEntity($responseEntity, $this->responseFactory, $this->streamFactory);
        } catch (Exception $exception) {
            throw $this->convertToClientException($exception, $request);
        } finally {
            $this->receivingHeadResponse = false;
        }
    }

    /**
     * 基于标准请求入口提供 SSE 事件流读取能力，支持任意 HTTP 方法（含 POST）。
     *
     * @return Generator<int, \Swow\Psr7\Message\EventStreamEvent>
     */
    public function sendEventStreamRequest(RequestInterface $request, ?int $timeout = null, int $readSize = 8192): Generator
    {
        $response = $this->sendRequest($request, $timeout);
        return Psr7::readEventStream($response->getBody(), $readSize);
    }

    public function upgradeToWebSocket(RequestInterface $request): ResponseInterface
    {
        $secWebSocketKey = base64_encode(random_bytes(16));
        $upgradeHeaders = [
            'Connection' => 'Upgrade',
            'Upgrade' => 'websocket',
            'Sec-WebSocket-Key' => $secWebSocketKey,
            'Sec-WebSocket-Version' => (string) WebSocket::VERSION,
        ];
        $request = Psr7::withHeaders($request, $upgradeHeaders);

        $response = $this->sendRequest($request);

        if ($response->getStatusCode() !== HttpStatus::SWITCHING_PROTOCOLS) {
            throw new ClientRequestException($request, $response->getReasonPhrase(), $response->getStatusCode());
        }
        // for performance (TODO: make it configurable)
        // if ($response->getHeaderLine('Sec-WebSocket-Accept') !== base64_encode(sha1($secWebSocketKey . WebSocket\GUID, true))) {
        //     throw new RequestException($request, 'Bad Sec-WebSocket-Accept');
        // }
        $this->upgraded(static::PROTOCOL_TYPE_WEBSOCKET);

        return $response;
    }

    protected function convertToClientException(Exception $exception, RequestInterface $request): ClientExceptionInterface
    {
        if ($exception instanceof SocketException) {
            return ClientNetworkException::fromException($exception, $request);
        } else {
            return ClientRequestException::fromException($exception, $request);
        }
    }
}
