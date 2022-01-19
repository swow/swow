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

use Swow\Http\Exception as HttpException;
use Swow\Http\Parser as HttpParser;
use Swow\Http\ReceiverTrait;
use Swow\Http\Server;
use Swow\Http\Status as HttpStatus;
use Swow\Http\TypeInterface as HttpTypeInterface;
use Swow\Http\TypeTrait as HttpTypeTrait;
use Swow\Http\WebSocketTrait;
use Swow\Socket;
use Swow\WebSocket;
use function base64_encode;
use function func_get_args;
use function is_array;
use function is_int;
use function is_string;
use function sha1;
use function strlen;
use function Swow\Http\packResponse;

class Connection extends Socket implements HttpTypeInterface
{
    use HttpTypeTrait;
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
        HttpParser::EVENT_BODY |
        HttpParser::EVENT_MESSAGE_COMPLETE;

    /* TODO: support chunk transfer encoding */

    /**
     * @noinspection PhpMissingParentConstructorInspection
     */
    public function __construct(protected Server $server)
    {
        $this->__constructReceiver(HttpParser::TYPE_REQUEST, static::DEFAULT_HTTP_PARSER_EVENTS);
    }

    public function getServer(): Server
    {
        return $this->server;
    }

    public function getType(): int
    {
        return $this->type;
    }

    public function getKeepAlive(): ?bool
    {
        return $this->keepAlive;
    }

    public function recvHttpRequest(?Request $request = null): Request
    {
        $result = $this->receiverExecute(
            $this->server->getMaxHeaderLength(),
            $this->server->getMaxContentLength()
        );

        $request ??= new Request();
        $request->setHead(
            $result->method,
            $result->uri,
            $result->protocolVersion,
            $result->headers,
            $result->headerNames,
            $result->shouldKeepAlive,
            $result->contentLength,
            $result->isUpgrade
        )->setBody($result->body);

        return $request;
    }

    public function sendHttpResponse(Response $response)
    {
        return $this->write([
            $response->toString(true),
            $response->getBodyAsString(),
        ]);
    }

    protected function generateResponseHeaders(string $body): array
    {
        $headers = [];
        $headers['Server'] = 'swow';
        if ($this->keepAlive !== null) {
            $headers['Connection'] = $this->keepAlive ? 'keep-alive' : 'close';
        }
        $headers['Content-Length'] = strlen($body);

        return $headers;
    }

    public function respond(...$args): void
    {
        switch ($this->type) {
            case static::TYPE_HTTP:
            {
                $statusCode = HttpStatus::OK;
                $headers = [];
                $body = '';
                foreach (func_get_args() as $arg) {
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
            case static::TYPE_WEBSOCKET:
            {
                // TODO: impl
                break;
            }
        }
    }

    public function error(int $code, string $message = ''): void
    {
        switch ($this->type) {
            case static::TYPE_HTTP:
            {
                if ($message === '') {
                    $message = HttpStatus::getReasonPhrase($code);
                }
                $message = "<html lang=\"en\"><body><h2>HTTP {$code} {$message}</h2><hr><i>Powered by Swow</i></body></html>";
                $this->write([
                    packResponse($code, $this->generateResponseHeaders($message)),
                    $message,
                ]);
                break;
            }
            case static::TYPE_WEBSOCKET:
            {
                // TODO: impl
                break;
            }
        }
    }

    public function upgradeToWebSocket(Request $request, ?Response $response = null): static
    {
        $secWebSocketKey = $request->getHeaderLine('sec-websocket-key');
        if (strlen($secWebSocketKey) !== WebSocket\SECRET_KEY_ENCODED_LENGTH) {
            throw new HttpException(HttpStatus::BAD_REQUEST, 'Invalid Secret Key');
        }
        $key = base64_encode(sha1($secWebSocketKey . WebSocket\GUID, true));

        $statusCode = HttpStatus::SWITCHING_PROTOCOLS;
        $headers = [
            'Connection' => 'Upgrade',
            'Upgrade' => 'websocket',
            'Sec-WebSocket-Accept' => $key,
            'Sec-WebSocket-Version' => WebSocket\VERSION,
        ];

        if ($response === null) {
            $this->respond($statusCode, $headers);
        } else {
            $this->sendHttpResponse($response->setStatus($statusCode)->setHeaders($headers));
        }

        $this->upgraded(static::TYPE_WEBSOCKET);

        return $this;
    }

    protected function offline(): void
    {
        if ($this->server) {
            $this->server->offline($this->getFd());
            $this->server = null;
        }
    }

    public function close(): bool
    {
        $ret = parent::close();
        $this->offline();

        return $ret;
    }

    public function __destruct()
    {
        $this->offline();
    }
}
