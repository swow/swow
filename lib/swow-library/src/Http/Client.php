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

namespace Swow\Http;

use Exception;
use Psr\Http\Client\ClientExceptionInterface;
use Psr\Http\Client\ClientInterface;
use Psr\Http\Message\RequestInterface;
use Psr\Http\Message\ResponseInterface;
use Swow\Http\Client\NetworkException;
use Swow\Http\Client\RequestException;
use Swow\Http\Parser as HttpParser;
use Swow\Http\Status as HttpStatus;
use Swow\Socket;
use Swow\SocketException;
use Swow\WebSocket;
use function base64_encode;
use function random_bytes;
use function strlen;

class Client extends Socket implements ClientInterface, ProtocolTypeInterface
{
    use ConfigTrait;
    use ProtocolTypeTrait;
    /*
     * @use ReceiverTrait<RawResponse>
     */
    use ReceiverTrait {
        __construct as __constructReceiver;
        execute as receiverExecute;
    }
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
    }

    public function connect(string $name, int $port = 0, ?int $timeout = null): static
    {
        $this->host = $name;

        return parent::connect($name, $port, $timeout);
    }

    /** @param array<string, array<string>> $headers */
    public function sendRaw(
        string $method = 'GET',
        string $path = '/',
        array $headers = [],
        string $body = '',
        string $protocolVersion = '1.1'
    ): static {
        return $this->write([
            packRequest(
                $method,
                $path,
                $headers,
                '',
                $protocolVersion
            ),
            $body,
        ]);
    }

    public function recvRaw(): RawResponse
    {
        return $this->receiverExecute(
            $this->maxHeaderLength,
            $this->maxContentLength
        );
    }

    public function sendRequest(RequestInterface $request, ?ResponseInterface $response = null): ResponseInterface
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

            $result = $this->sendRaw(
                $request->getMethod(),
                $request->getRequestTarget(),
                $headers,
                $body,
                $request->getProtocolVersion()
            )->recvRaw();

            // TODO: Repackage into util methods
            if ($response === null || $response instanceof Response) {
                $response ??= new Response(
                    $result->statusCode,
                    $result->headers,
                    $result->body,
                    $result->reasonPhrase,
                    $result->protocolVersion
                );
            } else {
                $response = $response
                    ->withProtocolVersion($result->protocolVersion)
                    ->withStatus($result->statusCode, $result->reasonPhrase)
                    ->withBody($result->body);
                foreach ($result->headers as $headerName => $headerParts) {
                    $response = $response->withHeader($headerName, $headerParts);
                }
            }
            return $response;
        } catch (Exception $exception) {
            throw $this->convertToClientException($exception, $request);
        }
    }

    public function upgradeToWebSocket(RequestInterface $request, ?ResponseInterface $response = null): ResponseInterface
    {
        $secWebSocketKey = base64_encode(random_bytes(16));
        $upgradeHeaders = [
            'Connection' => 'Upgrade',
            'Upgrade' => 'websocket',
            'Sec-WebSocket-Key' => $secWebSocketKey,
            'Sec-WebSocket-Version' => (string) WebSocket::VERSION,
        ];
        // TODO: Repackage into util methods
        if ($request instanceof Request) {
            $request = $request->withHeaders($upgradeHeaders);
        } else {
            foreach ($upgradeHeaders as $upgradeHeaderName => $upgradeHeaderValue) {
                $request = $request->withHeader($upgradeHeaderName, $upgradeHeaderValue);
            }
        }

        try {
            $response = $this->sendRequest($request, $response);
        } catch (Exception $exception) {
            throw $this->convertToClientException($exception, $request);
        }

        if ($response->getStatusCode() !== HttpStatus::SWITCHING_PROTOCOLS) {
            throw new RequestException($request, $response->getReasonPhrase(), $response->getStatusCode());
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
            return new NetworkException($request, $exception->getMessage(), $exception->getCode(), $exception);
        }

        return new RequestException($request, $exception->getMessage(), $exception->getCode(), $exception);
    }
}
