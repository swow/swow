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

namespace Swow\Psr7\Server;

use Psr\Http\Message\RequestInterface;
use Psr\Http\Message\ResponseInterface;
use Psr\Http\Message\ServerRequestInterface;
use Swow\Errno;
use Swow\Http\Http;
use Swow\Http\Message\ServerRequestEntity;
use Swow\Http\Parser as HttpParser;
use Swow\Http\Protocol\ProtocolTypeInterface;
use Swow\Http\Protocol\ProtocolTypeTrait;
use Swow\Http\Protocol\ReceiverTrait;
use Swow\Http\Status as HttpStatus;
use Swow\Psr7\Message\Response;
use Swow\Psr7\Message\ResponseException;
use Swow\Psr7\Message\WebSocketTrait;
use Swow\Psr7\Psr7;
use Swow\Socket;
use Swow\SocketException;
use Swow\WebSocket\WebSocket;
use TypeError;

use function base64_encode;
use function get_debug_type;
use function is_array;
use function is_int;
use function is_string;
use function sha1;
use function sprintf;
use function strlen;

class ServerConnection extends Socket implements ProtocolTypeInterface
{
    use ProtocolTypeTrait;

    /**
     * @use ReceiverTrait<ServerRequestEntity>
     */
    use ReceiverTrait;

    use ServerParamsTrait;

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

    public function getMaxHeaderLength(): int
    {
        return $this->server->getMaxHeaderLength();
    }

    public function getMaxContentLength(): int
    {
        return $this->server->getMaxContentLength();
    }

    public function recvServerRequestEntity(): ServerRequestEntity
    {
        return $this->recvMessageEntity();
    }

    public function recvHttpRequest(): ServerRequestInterface
    {
        return Psr7::createServerRequestFromEntity(
            $this->recvServerRequestEntity(),
            $this->server->getServerRequestFactory(),
            $this->server->getUriFactory(),
            $this->server->getStreamFactory(),
            $this->server->getUploadedFileFactory()
        );
    }

    public function sendHttpResponse(ResponseInterface $response): static
    {
        return $this->write(Psr7::convertResponseToVector($response));
    }

    /** @return array<string, string> */
    protected function generateResponseHeaders(string $body): array
    {
        $headers = [];
        $headers['Server'] = 'swow';
        if ($this->shouldKeepAlive !== null) {
            $headers['Connection'] = $this->shouldKeepAlive ? 'keep-alive' : 'close';
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
                    $this->write([Http::packResponse($statusCode, $headers), $body]);
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
                        $message = HttpStatus::getReasonPhraseOf($code);
                    }
                    $message = "<html lang=\"en\"><body><h2>HTTP {$code} {$message}</h2><hr><i>Powered by Swow</i></body></html>";
                    $this->write([
                        Http::packResponse($code, $this->generateResponseHeaders($message)),
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

        if ($response === null) {
            $this->respond($statusCode, $upgradeHeaders);
        } else {
            if ($response instanceof Response) {
                $response = $response->dup()->setStatus($statusCode)->setHeaders($upgradeHeaders);
            } else {
                $response = $response->withStatus($statusCode);
                foreach ($upgradeHeaders as $upgradeHeaderName => $upgradeHeaderValue) {
                    $response = $response->withHeader($upgradeHeaderName, $upgradeHeaderValue);
                }
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
