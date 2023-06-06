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

use InvalidArgumentException;
use Psr\Http\Message\RequestInterface;
use Psr\Http\Message\ResponseInterface;
use Psr\Http\Message\ServerRequestInterface;
use Stringable;
use Swow\Errno;
use Swow\Http\Http;
use Swow\Http\Message\ServerRequestEntity;
use Swow\Http\Mime\MimeType;
use Swow\Http\Parser as HttpParser;
use Swow\Http\Protocol\ProtocolException;
use Swow\Http\Protocol\ProtocolTypeInterface;
use Swow\Http\Protocol\ProtocolTypeTrait;
use Swow\Http\Protocol\ReceiverTrait;
use Swow\Http\Status as HttpStatus;
use Swow\Psr7\Message\ServerRequest;
use Swow\Psr7\Message\ServerRequestPlusInterface;
use Swow\Psr7\Protocol\WebSocketTrait;
use Swow\Psr7\Psr7;
use Swow\Socket;
use Swow\SocketException;
use Swow\WebSocket\WebSocket;
use TypeError;

use function base64_encode;
use function get_debug_type;
use function is_array;
use function is_bool;
use function is_int;
use function sha1;
use function sprintf;
use function strlen;
use function Swow\Debug\isStrictStringable;

use const PATHINFO_EXTENSION;

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

        // Inherited server configuration.
        $this->setRecvMessageTimeout($server->getRecvMessageTimeout());
    }

    /**
     * get server bound to this connection
     *
     * @throws SocketException when connection is shut down
     */
    public function getServer(): Server
    {
        return $this->server ?? throw new SocketException('Connection has been closed', Errno::EBADF);
    }

    public function getMaxHeaderLength(): int
    {
        return $this->getServer()->getMaxHeaderLength();
    }

    public function getMaxContentLength(): int
    {
        return $this->getServer()->getMaxContentLength();
    }

    public function recvServerRequestEntity(?int $timeout = null): ServerRequestEntity
    {
        return $this->recvMessageEntity($timeout);
    }

    /**
     * @return ServerRequestInterface|ServerRequestPlusInterface|ServerRequest
     */
    public function recvHttpRequest(?int $timeout = null): ServerRequestInterface
    {
        $server = $this->getServer();
        return Psr7::createServerRequestFromEntity(
            $this->recvServerRequestEntity($timeout),
            $server->getServerRequestFactory(),
            $server->getUriFactory(),
            $server->getStreamFactory(),
            $server->getUploadedFileFactory()
        );
    }

    public function sendHttpResponse(ResponseInterface $response): static
    {
        return $this->write(Psr7::convertResponseToVector($response));
    }

    /** @return array<string, string> */
    protected function generateResponseHeaders(string $body, ?bool $close): array
    {
        $headers = [];
        $close ??= !$this->shouldKeepAlive;
        $headers['Connection'] = $close ? 'close' : 'keep-alive';
        $headers['Content-Length'] = (string) strlen($body);

        return $headers;
    }

    /**
     * @param int|string|Stringable|array<string, string>|array<string, array<string>>|bool $args
     */
    public function respond(mixed ...$args): void
    {
        switch ($this->protocolType) {
            case static::PROTOCOL_TYPE_HTTP:
                $statusCode = HttpStatus::OK;
                $headers = [];
                $body = '';
                $close = null;
                foreach ($args as $arg) {
                    if (isStrictStringable($arg)) {
                        $body = (string) $arg;
                    } elseif (is_int($arg)) {
                        $statusCode = $arg;
                    } elseif (is_array($arg)) {
                        $headers = $arg;
                    } elseif (is_bool($arg)) {
                        $close = $arg;
                    } else {
                        throw new TypeError(sprintf('Unsupported argument type %s', get_debug_type($arg)));
                    }
                }
                $headers += $this->generateResponseHeaders($body, $close);
                $this->write([
                    Http::packResponse(
                        statusCode: $statusCode,
                        headers: $headers
                    ),
                    $body,
                ]);
                break;
            case static::PROTOCOL_TYPE_WEBSOCKET:
                // TODO: impl
                break;
        }
    }

    public function error(int $statusCode, string $message = '', ?bool $close = null): void
    {
        switch ($this->protocolType) {
            case static::PROTOCOL_TYPE_HTTP:
                if ($message === '') {
                    $message = HttpStatus::getReasonPhraseOf($statusCode);
                }
                $message = "<html lang=\"en\"><body><h2>HTTP {$statusCode} {$message}</h2><hr><i>Powered by Swow</i></body></html>";
                $this->write([
                    Http::packResponse(
                        statusCode: $statusCode,
                        headers: $this->generateResponseHeaders($message, $close)
                    ),
                    $message,
                ]);
                break;
            case static::PROTOCOL_TYPE_WEBSOCKET:
                // TODO: impl
                break;
        }
    }

    public function upgradeToWebSocket(RequestInterface $request, ?ResponseInterface $response = null): static
    {
        $secWebSocketKey = $request->getHeaderLine('sec-websocket-key');
        if (strlen($secWebSocketKey) !== WebSocket::SECRET_KEY_ENCODED_LENGTH) {
            throw new ProtocolException(HttpStatus::BAD_REQUEST, 'Invalid Secret Key');
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
            Psr7::setStatus($response, $statusCode);
            Psr7::setHeaders($response, $upgradeHeaders);
            $this->sendHttpResponse($response);
        }
        $this->upgraded(static::PROTOCOL_TYPE_WEBSOCKET);

        return $this;
    }

    protected function offline(): void
    {
        $server = $this->server;
        if ($server) {
            $this->server = null;
            $server->offline($this);
        }
    }

    public function close(): bool
    {
        $this->offline();

        return parent::close();
    }

    public function sendHttpFile(ResponseInterface $response, string $filename, int $offset = 0, int $length = -1, ?int $timeout = null): int
    {
        if ($response->hasHeader('Content-Length')) {
            throw new InvalidArgumentException('Content-Length cannot be set');
        }

        $headers = $response->getHeaders();

        if (!$response->hasHeader('Content-Type')) {
            $extension = pathinfo($filename, PATHINFO_EXTENSION);
            $headers['Content-Type'] = MimeType::fromExtension($extension);
        }

        $headers['Content-Length'] = $length <= 0 ? filesize($filename) : $length;
        $this->send(Http::packResponse(
            statusCode: HttpStatus::OK,
            headers: $headers
        ));

        return $this->sendFile($filename, $offset, $length, $timeout);
    }
}
