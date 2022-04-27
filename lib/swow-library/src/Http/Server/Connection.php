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

namespace Swow\Http\Server;

use Psr\Http\Message\RequestInterface;
use Psr\Http\Message\ResponseInterface;
use Stringable;
use Swow\Errno;
use Swow\Http\Buffer;
use Swow\Http\Parser as HttpParser;
use Swow\Http\ProtocolTypeInterface;
use Swow\Http\ProtocolTypeTrait;
use Swow\Http\ReceiverTrait;
use Swow\Http\ResponseException;
use Swow\Http\Server;
use Swow\Http\Status as HttpStatus;
use Swow\Http\WebSocketTrait;
use Swow\Socket;
use Swow\SocketException;
use Swow\WebSocket;
use TypeError;
use function base64_encode;
use function get_debug_type;
use function is_array;
use function is_int;
use function is_string;
use function sha1;
use function sprintf;
use function strlen;
use function Swow\Http\packResponse;

class Connection extends Socket implements ProtocolTypeInterface
{
    use ProtocolTypeTrait;
    use ReceiverTrait {
        __construct as __constructReceiver;
        execute as receiverExecute;
    }
    use WebSocketTrait;

    public const DEFAULT_HTTP_PARSER_EVENTS =
        HttpParser::EVENT_URL |
        HttpParser::EVENT_HEADER_FIELD |
        HttpParser::EVENT_HEADER_VALUE |
        HttpParser::EVENT_HEADERS_COMPLETE |
        HttpParser::EVENT_CHUNK_HEADER |
        HttpParser::EVENT_CHUNK_COMPLETE |
        HttpParser::EVENT_BODY |
        HttpParser::EVENT_MESSAGE_COMPLETE |
        /* multipart */
        HttpParser::EVENT_MULTIPART_HEADER_FIELD |
        HttpParser::EVENT_MULTIPART_HEADER_VALUE |
        HttpParser::EVENT_MULTIPART_HEADERS_COMPLETE |
        HttpParser::EVENT_MULTIPART_BODY |
        HttpParser::EVENT_MULTIPART_DATA_END;

    /* TODO: support chunk transfer encoding */

    protected ?Server $server;

    /** @var array{'remote_addr': string, 'remote_port': int} */
    protected array $serverParams;

    public function __construct(Server $server)
    {
        parent::__construct($server->getSimpleType());
        $this->__constructReceiver(HttpParser::TYPE_REQUEST, static::DEFAULT_HTTP_PARSER_EVENTS);
        $this->server = $server;
    }

    /**
     * get server bound to this connection
     *
     * @throws SocketException when connection is shut down
     */
    public function getServer(): Server
    {
        return $this->server ?? throw new SocketException('Connection is shut down', Errno::EBADF);
    }

    public function getProtocolType(): int
    {
        return $this->protocolType;
    }

    public function getKeepAlive(): ?bool
    {
        return $this->keepAlive;
    }

    public function recvHttpRequest(): Request
    {
        return $this->recvHttpRequestTo(new Request());
    }

    public function recvHttpRequestTo(RequestInterface $request): Request
    {
        $result = $this->receiverExecute(
            $this->server->getMaxHeaderLength(),
            $this->server->getMaxContentLength()
        );
        $result->serverParams = ($this->serverParams ??= [
            'remote_addr' => $this->getPeerAddress(),
            'remote_port' => $this->getPeerPort(),
        ]);

        // TODO: Repackage into util methods
        if ($request instanceof Request) {
            $request->constructFromRawRequest($result);
        }
        // TODO: support the other kinds of Psr7 Request

        return $request;
    }

    public function sendHttpResponse(ResponseInterface $response): static
    {
        // TODO: Repackage into util methods
        if ($response instanceof \Swow\Http\Response) {
            return $this->write([
                $response->toString(true),
                (string) $response->getBody(),
            ]);
        }
        if ($response instanceof Stringable &&
            $response->getBody()->getSize() <= Buffer::PAGE_SIZE) {
            return $this->sendString((string) $response);
        }
        return $this->write([
            packResponse(
                $response->getStatusCode(),
                $response->getHeaders(),
                $response->getReasonPhrase(),
                $response->getProtocolVersion()
            ),
            (string) $response->getBody(),
        ]);
    }

    /** @return array<string, string> */
    protected function generateResponseHeaders(string $body): array
    {
        $headers = [];
        $headers['Server'] = 'swow';
        if ($this->keepAlive !== null) {
            $headers['Connection'] = $this->keepAlive ? 'keep-alive' : 'close';
        }
        $headers['Content-Length'] = (string) strlen($body);

        return $headers;
    }

    /**
     * @param int|string|array<string, string>|array<string, array<string>> $args
     */
    public function respond(mixed ...$args): void
    {
        switch ($this->protocolType) {
            case static::PROTOCOL_TYPE_HTTP:
            {
                $statusCode = HttpStatus::OK;
                $headers = [];
                $body = '';
                foreach ($args as $arg) {
                    if (is_string($arg)) {
                        $body = $arg;
                    } elseif (is_int($arg)) {
                        $statusCode = $arg;
                    } elseif (is_array($arg)) {
                        $headers = $arg;
                    } else {
                        throw new TypeError(sprintf('Unsupported argument type %s', get_debug_type($arg)));
                    }
                }
                $headers += $this->generateResponseHeaders($body);
                $this->write([packResponse($statusCode, $headers), $body]);
                break;
            }
            case static::PROTOCOL_TYPE_WEBSOCKET:
            {
                // TODO: impl
                break;
            }
        }
    }

    public function error(int $code, string $message = ''): void
    {
        switch ($this->protocolType) {
            case static::PROTOCOL_TYPE_HTTP:
            {
                if ($message === '') {
                    $message = HttpStatus::getReasonPhraseFor($code);
                }
                $message = "<html lang=\"en\"><body><h2>HTTP {$code} {$message}</h2><hr><i>Powered by Swow</i></body></html>";
                $this->write([
                    packResponse($code, $this->generateResponseHeaders($message)),
                    $message,
                ]);
                break;
            }
            case static::PROTOCOL_TYPE_WEBSOCKET:
            {
                // TODO: impl
                break;
            }
        }
    }

    public function upgradeToWebSocket(RequestInterface $request, ?ResponseInterface $response = null): static
    {
        $secWebSocketKey = $request->getHeaderLine('sec-websocket-key');
        if (strlen($secWebSocketKey) !== WebSocket::SECRET_KEY_ENCODED_LENGTH) {
            throw new ResponseException(HttpStatus::BAD_REQUEST, 'Invalid Secret Key');
        }
        $key = base64_encode(sha1($secWebSocketKey . WebSocket::GUID, true));

        $statusCode = HttpStatus::SWITCHING_PROTOCOLS;
        $upgradeHeaders = [
            'Connection' => 'Upgrade',
            'Upgrade' => 'websocket',
            'Sec-WebSocket-Accept' => $key,
            'Sec-WebSocket-Version' => (string) WebSocket::VERSION,
        ];

        // TODO: Repackage into util methods
        if ($response === null) {
            $this->respond($statusCode, $upgradeHeaders);
        } elseif ($response instanceof \Swow\Http\Response) {
            $this->sendHttpResponse($response->dup()->setStatus($statusCode)->setHeaders($upgradeHeaders));
        } else {
            $response = $response->withStatus($statusCode);
            foreach ($upgradeHeaders as $upgradeHeaderName => $upgradeHeaderValue) {
                $response = $response->withHeader($upgradeHeaderName, $upgradeHeaderValue);
            }
            $this->sendHttpResponse($response);
        }

        $this->upgraded(static::PROTOCOL_TYPE_WEBSOCKET);

        return $this;
    }

    protected function offline(): void
    {
        if ($this->server) {
            $this->server->offline($this->getId());
            $this->server = null;
        }
    }

    public function close(): bool
    {
        $this->offline();

        return parent::close();
    }

    public function __destruct()
    {
        $this->offline();
    }
}
