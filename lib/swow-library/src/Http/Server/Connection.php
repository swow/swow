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

use Swow\Errno;
use Swow\Http\Parser as HttpParser;
use Swow\Http\ProtocolTypeInterface;
use Swow\Http\ProtocolTypeTrait;
use Swow\Http\ReceiverTrait;
use Swow\Http\ResponseException;
use Swow\Http\Server;
use Swow\Http\Status as HttpStatus;
use Swow\Http\UploadFile;
use Swow\Http\WebSocketTrait;
use Swow\Socket;
use Swow\SocketException;
use Swow\WebSocket;
use function base64_encode;
use function is_array;
use function is_int;
use function is_string;
use function sha1;
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
        return $this->server ?? throw new SocketException(message: 'Connection is shut down', code: Errno::EBADF);
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

    public function recvHttpRequestTo(Request $request): Request
    {
        $result = $this->receiverExecute(
            $this->server->getMaxHeaderLength(),
            $this->server->getMaxContentLength()
        );
        $result->serverParams = ($this->serverParams ??= [
            'remote_addr' => $this->getPeerAddress(),
            'remote_port' => $this->getPeerPort(),
        ]);

        $request->setHead(
            $result->method,
            $result->uri,
            $result->protocolVersion,
            $result->headers,
            $result->headerNames,
            $result->shouldKeepAlive,
            $result->contentLength,
            $result->isUpgrade,
            $result->serverParams,
        )->setBody($result->body);
        if ($result->uploadedFiles) {
            $uploadedFiles = [];
            foreach ($result->uploadedFiles as $rawUploadedFile) {
                $uploadedFiles[] = new UploadFile(
                    $rawUploadedFile->tmp_name,
                    $rawUploadedFile->size,
                    $rawUploadedFile->error,
                    $rawUploadedFile->name,
                    $rawUploadedFile->type
                );
            }
            $request->setUploadedFiles($uploadedFiles);
        }

        return $request;
    }

    public function sendHttpResponse(Response $response): static
    {
        return $this->write([
            $response->toString(true),
            $response->getBodyAsString(),
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

    public function upgradeToWebSocket(Request $request, ?Response $response = null): static
    {
        $secWebSocketKey = $request->getHeaderLine('sec-websocket-key');
        if (strlen($secWebSocketKey) !== WebSocket::SECRET_KEY_ENCODED_LENGTH) {
            throw new ResponseException(HttpStatus::BAD_REQUEST, 'Invalid Secret Key');
        }
        $key = base64_encode(sha1($secWebSocketKey . WebSocket::GUID, true));

        $statusCode = HttpStatus::SWITCHING_PROTOCOLS;
        $headers = [
            'Connection' => 'Upgrade',
            'Upgrade' => 'websocket',
            'Sec-WebSocket-Accept' => $key,
            'Sec-WebSocket-Version' => (string) WebSocket::VERSION,
        ];

        if ($response === null) {
            $this->respond($statusCode, $headers);
        } else {
            $this->sendHttpResponse($response->setStatus($statusCode)->setHeaders($headers));
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
