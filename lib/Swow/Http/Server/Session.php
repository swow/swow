<?php
/**
 * This file is part of Swow
 *
 * @link     https://github.com/swow/swow
 * @contact  twosee <twosee@php.net>
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
use Swow\Socket;
use Swow\WebSocket;
use function Swow\Http\packResponse;

class Session extends Socket
{
    use ReceiverTrait {
        __construct as receiverConstruct;
        execute as receiverExecute;
    }

    public const TYPE_HTTP = 1 << 0;

    public const TYPE_WEBSOCKET = 1 << 1;

    public const TYPE_HTTP2 = 1 << 2;

    public const DEFAULT_HTTP_PARSER_EVENTS =
        HttpParser::EVENT_URL |
        HttpParser::EVENT_HEADER_FIELD |
        HttpParser::EVENT_HEADER_VALUE |
        HttpParser::EVENT_HEADERS_COMPLETE |
        HttpParser::EVENT_BODY |
        HttpParser::EVENT_MESSAGE_COMPLETE;

    /* TODO: support chunk transfer encoding */

    /**
     * @var Server
     */
    protected $server;

    /**
     * @var int
     */
    protected $type = self::TYPE_HTTP;

    /**
     * @var null|bool
     */
    protected $keepAlive = false;

    /**
     * @noinspection PhpMissingParentConstructorInspection
     */
    public function __construct()
    {
        $this->receiverConstruct(HttpParser::TYPE_REQUEST, static::DEFAULT_HTTP_PARSER_EVENTS);
    }

    public function getServer(): Server
    {
        return $this->server;
    }

    public function setServer(Server $server): self
    {
        $this->server = $server;

        return $this;
    }

    public function getType(): int
    {
        return $this->type;
    }

    public function getKeepAlive(): ?bool
    {
        return $this->keepAlive;
    }

    public function recvHttpRequest(Request $request = null): Request
    {
        $result = $this->receiverExecute(
            $this->server->getMaxHeaderLength(),
            $this->server->getMaxContentLength()
        );

        $request = $request ?? new Request();
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

    public function sendHttpResponse(Response $response): self
    {
        $this->write([
            $response->toString(true),
            $response->getBodyAsString(),
        ]);

        return $this;
    }

    protected function generateResponseHeaders(string $body): array
    {
        $headers = [];
        $headers['Server'] = 'swow';
        if ($this->keepAlive !== null) {
            $headers['Connection'] = $this->keepAlive ? 'Keep-Alive' : 'Closed';
        }
        $headers['Content-Length'] = strlen($body);

        return $headers;
    }

    public function respond(...$args)
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

    public function error(int $code, string $message = '')
    {
        switch ($this->type) {
            case static::TYPE_HTTP:
            {
                if ($message === '') {
                    $message = HttpStatus::getReasonPhrase($code);
                }
                $this->write([
                    packResponse($code, $this->generateResponseHeaders($message)),
                    "<html lang=\"en\"><body><h2>HTTP {$code} {$message}</h2><hr><i>Powered by Swow</i></body></html>\r\n",
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

    protected function upgraded(int $type)
    {
        $this->type = $type;
        /* release useless resources */
        // if (!($type & static::TYPE_HTTP)) {
        $this->httpParser = null;
        // }
    }

    public function upgradeToWebSocket(Request $request, Response $response = null): self
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

    public function recvWebSocketFrame(WebSocketFrame $frame = null): WebSocketFrame
    {
        $frame = $frame ?? new WebSocketFrame();
        $buffer = $this->buffer;
        $expectMore = $buffer->eof();
        while (true) {
            if ($expectMore) {
                /* ltrim (data left from the last request) */
                $this->recvData($buffer);
            }
            $headerLength = $frame->unpackHeader($buffer);
            if ($headerLength === 0) {
                $expectMore = true;
                continue;
            }
            $payloadLength = $frame->getPayloadLength();
            if ($payloadLength > 0) {
                $payloadData = $frame->getPayloadData();
                $writableSize = $payloadData->getWritableSize();
                if ($writableSize < $payloadLength) {
                    $payloadData->realloc($payloadData->tell() + $payloadLength);
                }
                if (!$buffer->eof()) {
                    $copyLength = min($buffer->getReadableLength(), $payloadLength);
                    /* TODO: $payloadData->copy($buffer, $copyLength); */
                    $payloadData->write($buffer->toString(), $buffer->tell(), $copyLength);
                    $buffer->seek($copyLength, SEEK_CUR);
                    $payloadLength -= $copyLength;
                }
                if ($payloadLength > 0) {
                    $this->read($payloadData, $payloadLength);
                }
                if ($frame->getMask()) {
                    $frame->unmaskPayloadData();
                } /* else {
                    // TODO: bad frame
                } */
                $payloadData->rewind();
            }
            break;
        }

        return $frame;
    }

    public function sendWebSocketFrame(WebSocketFrame $frame)
    {
        $this->write([
            $frame->toString(true),
            $frame->getPayloadDataAsString(),
        ]);
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
