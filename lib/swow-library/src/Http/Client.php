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

use Psr\Http\Client\ClientExceptionInterface;
use Psr\Http\Client\ClientInterface;
use Psr\Http\Message\RequestInterface;
use Psr\Http\Message\ResponseInterface;
use Swow\Http\Client\NetworkException;
use Swow\Http\Client\RequestException;
use Swow\Http\Parser as HttpParser;
use Swow\Http\Status as HttpStatus;
use Swow\Http\TypeInterface as HttpTypeInterface;
use Swow\Socket;
use Swow\Socket\Exception as SocketException;
use Swow\WebSocket;
use function base64_encode;
use function random_bytes;
use function strlen;

class Client extends Socket implements ClientInterface, HttpTypeInterface
{
    use TypeTrait;
    use ConfigTrait;
    use ReceiverTrait {
        __construct as receiverConstruct;
        execute as receiverExecute;
    }
    use WebSocketTrait;

    public const DEFAULT_HTTP_PARSER_EVENTS =
        HttpParser::EVENT_STATUS |
        HttpParser::EVENT_HEADER_FIELD |
        HttpParser::EVENT_HEADER_VALUE |
        HttpParser::EVENT_HEADERS_COMPLETE |
        HttpParser::EVENT_BODY |
        HttpParser::EVENT_MESSAGE_COMPLETE;

    protected string $host = '';

    public function __construct(int $type = Socket::TYPE_TCP)
    {
        parent::__construct($type);
        $this->receiverConstruct(Parser::TYPE_RESPONSE, static::DEFAULT_HTTP_PARSER_EVENTS);
    }

    /** @return $this */
    public function connect(string $name, int $port = 0, ?int $timeout = null): static
    {
        $this->host = $name;

        return parent::connect($name, $port, $timeout);
    }

    /** @return $this */
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

    /**
     * @return Response|ResponseInterface
     */
    public function sendRequest(RequestInterface $request): ResponseInterface
    {
        try {
            $headers = $request->getHeaders();
            $body = (string) $request->getBody();
            if (!$request->hasHeader('host')) {
                $headers['Host'] = $this->host;
            }
            if (!$request->hasHeader('content-length')) {
                $headers['Content-Length'] = strlen($body);
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

            return new Response(
                $result->statusCode,
                $result->headers,
                $result->body,
                $result->reasonPhrase,
                $result->protocolVersion
            );
        } catch (Exception $exception) {
            throw $this->convertToClientException($exception, $request);
        }
    }

    public function upgradeToWebSocket(Request $request): Response
    {
        $secWebSocketKey = base64_encode(random_bytes(16));
        $request = $request->withHeaders([
            'Connection' => 'Upgrade',
            'Upgrade' => 'websocket',
            'Sec-WebSocket-Key' => $secWebSocketKey,
            'Sec-WebSocket-Version' => WebSocket\VERSION,
        ]);
        try {
            $response = $this->sendRequest($request);
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
        $this->upgraded(static::TYPE_WEBSOCKET);

        return $response;
    }

    protected function convertToClientException(\Exception $exception, RequestInterface $request): ClientExceptionInterface
    {
        if ($exception instanceof SocketException) {
            return new NetworkException($request, $exception->getMessage(), $exception->getCode());
        }

        return new RequestException($request, $exception->getMessage(), $exception->getCode());
    }
}
