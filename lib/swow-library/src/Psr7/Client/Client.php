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
use Psr\Http\Client\ClientExceptionInterface;
use Psr\Http\Client\ClientInterface;
use Psr\Http\Message\RequestInterface;
use Psr\Http\Message\ResponseInterface;
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
use Swow\Psr7\Message\WebSocketTrait;
use Swow\Psr7\Psr7;
use Swow\Socket;
use Swow\SocketException;
use Swow\WebSocket\WebSocket;

use function base64_encode;
use function random_bytes;
use function strlen;

class Client extends Socket implements ClientInterface, ProtocolTypeInterface
{
    use ClientPsr17FactoryTrait;
    use LimitationTrait;

    use ProtocolTypeTrait;

    /**
     * @use ReceiverTrait<ResponseEntity>
     */
    use ReceiverTrait;

    use WebSocketTrait;

    public const DEFAULT_HTTP_PARSER_EVENTS =
        HttpParser::EVENT_STATUS |
        HttpParser::EVENT_HEADER_FIELD |
        HttpParser::EVENT_HEADER_VALUE |
        HttpParser::EVENT_HEADERS_COMPLETE |
        HttpParser::EVENT_CHUNK_HEADER |
        HttpParser::EVENT_CHUNK_COMPLETE |
        HttpParser::EVENT_BODY |
        HttpParser::EVENT_MESSAGE_COMPLETE;

    protected string $host = '';

    public function __construct(int $type = Socket::TYPE_TCP)
    {
        parent::__construct($type);
        $this->__constructReceiver(Parser::TYPE_RESPONSE, static::DEFAULT_HTTP_PARSER_EVENTS);
        $this->__constructClientPsr17Factory();
    }

    public function connect(string $name, int $port = 0, ?int $timeout = null): static
    {
        $this->host = $name;

        return parent::connect($name, $port, $timeout);
    }

    /** @param array<string, array<string>> $headers */
    public function sendPackedRequestAsync(
        string $protocolVersion = Request::DEFAULT_PROTOCOL_VERSION,
        string $method = 'GET',
        string $url = '/',
        array $headers = [],
        string $body = '',
    ): static {
        return $this->write([
            Http::packRequest(
                method: $method,
                url: $url,
                headers: $headers,
                protocolVersion: $protocolVersion
            ),
            $body,
        ]);
    }

    public function recvResponseEntity(): ResponseEntity
    {
        return $this->recvMessageEntity();
    }

    public function sendRequest(RequestInterface $request): ResponseInterface
    {
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

            $responseEntity = $this->sendPackedRequestAsync(
                protocolVersion: $request->getProtocolVersion(),
                method: $request->getMethod(),
                url: $request->getRequestTarget(),
                headers: $headers,
                body: $body,
            )->recvResponseEntity();

            return Psr7::createResponseFromEntity($responseEntity, $this->responseFactory, $this->streamFactory);
        } catch (Exception $exception) {
            throw $this->convertToClientException($exception, $request);
        }
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
        Psr7::withHeaders($request, $upgradeHeaders);

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
