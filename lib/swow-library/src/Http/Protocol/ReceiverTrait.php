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

namespace Swow\Http\Protocol;

use Swow\Buffer;
use Swow\Coroutine;
use Swow\Errno;
use Swow\Exception\ExceptionEditor;
use Swow\Http\Message\ResponseEntity;
use Swow\Http\Message\ServerRequestEntity;
use Swow\Http\Message\UploadedFileEntity;
use Swow\Http\Message\WebSocketFrameEntity;
use Swow\Http\Mime\MimeType;
use Swow\Http\Parser as HttpParser;
use Swow\Http\ParserException;
use Swow\Http\Status as HttpStatus;
use Swow\SocketException;
use Swow\WebSocket\WebSocket;
use Throwable;
use ValueError;

use function array_filter;
use function array_map;
use function count;
use function explode;
use function fopen;
use function fwrite;
use function implode;
use function in_array;
use function max;
use function parse_str;
use function sprintf;
use function strcasecmp;
use function strtolower;
use function sys_get_temp_dir;
use function tempnam;
use function trim;

use const PHP_INT_MAX;
use const UPLOAD_ERR_CANT_WRITE;
use const UPLOAD_ERR_OK;

/**
 * http response / request receiver
 *
 * @phan-template T of ServerRequestEntity|ResponseEntity
 * @phpstan-template T of ServerRequestEntity|ResponseEntity
 * @psalm-template T of ServerRequestEntity|ResponseEntity
 */
trait ReceiverTrait
{
    protected Buffer $buffer;

    protected ?HttpParser $httpParser = null;

    protected int $parsedOffset = 0;

    protected float $bufferLoadFactor = 0.75;

    protected bool $preserveBodyData = false;

    protected bool $autoUnmask = true;

    protected int $recvMessageTimeout = -1;

    protected bool $shouldKeepAlive = false;

    protected bool $streamingChunkedResponse = false;

    protected ?ChunkedBodyState $activeChunkedBody = null;

    protected function __constructReceiver(int $type, int $events): void
    {
        $this->buffer = new Buffer(max(Buffer::COMMON_SIZE, $this->getMaxHeaderLength()));
        $requiredEvents =
            HttpParser::EVENT_URL |
            HttpParser::EVENT_STATUS |
            HttpParser::EVENT_HEADER_FIELD |
            HttpParser::EVENT_HEADER_VALUE |
            HttpParser::EVENT_HEADERS_COMPLETE |
            HttpParser::EVENT_CHUNK_HEADER |
            HttpParser::EVENT_CHUNK_COMPLETE |
            HttpParser::EVENT_BODY |
            HttpParser::EVENT_MESSAGE_COMPLETE;
        $this->httpParser = (new HttpParser())
            ->setType($type)
            ->setEvents($events | $requiredEvents);
    }

    public function getBufferLoadFactor(): float
    {
        return $this->bufferLoadFactor;
    }

    public function setBufferLoadFactor(float $bufferLoadFactor): static
    {
        if ($bufferLoadFactor < 0 || $bufferLoadFactor > 1) {
            throw new ValueError(sprintf(
                '%s(): Argument#1 ($bufferLoadFactor) should be between 0 and 1',
                __METHOD__
            ));
        }
        $this->bufferLoadFactor = $bufferLoadFactor;

        return $this;
    }

    public function isPreserveBodyData(): bool
    {
        return $this->preserveBodyData;
    }

    public function setPreserveBodyData(bool $enable): static
    {
        $this->preserveBodyData = $enable;

        return $this;
    }

    /**
     * @return bool Whether unmask WebSocket payload data automatically
     */
    public function isAutoUnmask(): bool
    {
        return $this->autoUnmask;
    }

    /**
     * @param bool $enable If true, WebSocket payload data will be unmasked automatically
     */
    public function setAutoUnmask(bool $enable): static
    {
        $this->autoUnmask = $enable;

        return $this;
    }

    public function getRecvMessageTimeout(): int
    {
        return $this->recvMessageTimeout;
    }

    /**
     * @param int $timeout HTTP keep alive timeout in milliseconds
     */
    public function setRecvMessageTimeout(int $timeout): static
    {
        $this->recvMessageTimeout = $timeout;

        return $this;
    }

    public function shouldKeepAlive(): bool
    {
        return $this->shouldKeepAlive;
    }

    public function isStreamingChunkedResponse(): bool
    {
        return $this->streamingChunkedResponse;
    }

    public function setStreamingChunkedResponse(bool $enable): static
    {
        $this->streamingChunkedResponse = $enable;
        return $this;
    }

    /**
     * @TODO The options must be managed in a unified way
     * @phan-return T
     * @phpstan-return T
     * @psalm-return T
     */
    protected function recvMessageEntity(?int $timeout = null): ServerRequestEntity|ResponseEntity
    {
        // 同一连接上不能带着“上一条未消费完成 body”继续解析下一条消息。
        $this->guardPreviousChunkedBody();
        $thisBuffer = $this->buffer;
        $thisBufferParsedOffset = null;
        /* buffer may be replaced for some special parsing cases,
         * e.g. preserve body data case. */
        $buffer = $thisBuffer;
        $parser = $this->httpParser;
        $parsedOffset = $this->parsedOffset;
        $isServerRequest = $parser->getType() === HttpParser::TYPE_REQUEST;
        $messageEntity = $isServerRequest ? new ServerRequestEntity() : new ResponseEntity();
        $maxHeaderLength = $this->getMaxHeaderLength();
        $maxContentLength = $this->getMaxContentLength();

        /* Socket IO related values {{{ */
        $expectMoreData = $parsedOffset === $buffer->getLength();
        /* }}} */
        /* HTTP parser related values {{{ */
        $event = HttpParser::EVENT_NONE;
        $dataOffset = $dataLength = 0;
        $data = '';
        /* }}} */
        /* HTTP related values {{{ */
        $uriOrReasonPhrase = '';
        $headerName = '';
        $formDataName = '';
        $fileName = '';
        /** @var array<string, array<string>> $headers */
        $headers = [];
        /** @var array<string, string> $headerNames */
        $headerNames = [];
        $shouldKeepAlive = false;
        $contentLength = 0;
        $headerLength = 0;
        $headersCompleted = false;
        $isChunked = false;
        $currentChunkLength = 0;
        $body = null;
        $isStreamingChunkedBody = false;
        /* }}} */
        /* multipart related values {{{ */
        $isMultipart = false;
        $multiPartHeadersCompleted = false;
        $multipartHeaderName = '';
        $multipartHeaders = [];
        $tmpName = '';
        $tmpFile = null;
        $tmpFileSize = 0;
        $fileError = UPLOAD_ERR_OK;
        $formData = [];
        $uploadedFiles = [];
        /* }}} */
        $timeout ??= $this->recvMessageTimeout;
        $readTimeout = $this->getReadTimeout();
        // if read timeout is less than message timeout, use message timeout instead
        if (($timeout < 0 ? PHP_INT_MAX : $timeout) < ($readTimeout < 0 ? PHP_INT_MAX : $timeout)) {
            $readTimeout = $timeout;
        }
        $mainCoroutine = Coroutine::getMain();
        try {
            while (true) {
                if ($expectMoreData) {
                    if ($timeout >= 0) {
                        if (!isset($startTime)) {
                            $startTime = $mainCoroutine->getElapsed();
                        } else {
                            $timePassed = $mainCoroutine->getElapsed() - $startTime;
                            if ($timePassed > $timeout) {
                                throw new SocketException(sprintf('Recv HTTP message timeout (expect within %d ms, but %d ms passed)', $timeout, $timePassed), Errno::ETIMEDOUT);
                            }
                            $readTimeout = $timeout - $timePassed;
                        }
                    }
                    $this->recvData($buffer, offset: $buffer->getLength(), timeout: $readTimeout);
                    /** @noinspection PhpUnusedLocalVariableInspection (on the safe-side) */
                    $expectMoreData = false;
                }
                // TODO: call $parser->finished() if connection error?
                while (true) {
                    $previousEvent = $event;
                    $parsedLength = $parser->execute($buffer, $parsedOffset);
                    $parsedOffset += $parsedLength;
                    $event = $parser->getEvent();
                    if ($event & HttpParser::EVENT_FLAG_DATA) {
                        $dataOffset = $parser->getDataOffset();
                        $dataLength = $parser->getDataLength();
                        if (!$headersCompleted || ($isMultipart && !$multiPartHeadersCompleted)) {
                            $data = $buffer->read($dataOffset, $dataLength);
                        }
                    }
                    if (!$headersCompleted) {
                        $headerLength += $parsedLength;
                        if ($headerLength > $maxHeaderLength) {
                            throw new ProtocolException(empty($headerName) ? HttpStatus::REQUEST_URI_TOO_LARGE : HttpStatus::REQUEST_HEADER_FIELDS_TOO_LARGE);
                        }
                    }
                    if ($event === HttpParser::EVENT_NONE) {
                        if ($buffer !== $thisBuffer) {
                            throw new ParserException('Unexpected EVENT_NONE, buffer is dummy one');
                        }
                        $buffer->truncateFrom($parsedOffset);
                        if ($buffer->isFull()) {
                            throw new ParserException('Buffer is full and unable to continue parsing');
                        }
                        $parsedOffset = 0;
                        $expectMoreData = true;
                        break; /* goto recv more data */
                    }
                    if ($event === HttpParser::EVENT_MESSAGE_COMPLETE) {
                        if ($isChunked) {
                            // Remove 'chunked' field from Transfer-Encoding header because we combined the chunked body
                            $transferEncodingIndex = $headerNames['transfer-encoding'];
                            $headers[$transferEncodingIndex] = implode(', ', array_filter(
                                array_map('trim', explode(',', implode(',', $headers[$transferEncodingIndex]))),
                                static function (string $value): bool {
                                    return strcasecmp($value, 'chunked') !== 0;
                                }
                            ));
                            $headers[$headerNames['content-length'] ??= 'Content-Length'] = $body ? (string) $body->getLength() : '0';
                            if ($body && $body->getLength() < ($body->getSize() / 2)) {
                                $body->mallocTrim();
                            }
                        } elseif ($isMultipart && !$this->preserveBodyData) {
                            // Some program may want to get content length header,
                            // we would find it strange if we didn't consider them to have a body.
                            // $contentLengthIndex = $headerNames['content-length'] ?? null;
                            // if ($contentLengthIndex) {
                            //     unset($headerNames['content-length']);
                            //     unset($headers[$contentLengthIndex]);
                            // }
                        }
                        break 2;
                    }
                    if (!$headersCompleted) {
                        switch ($event) {
                            case HttpParser::EVENT_HEADER_FIELD:
                                if ($event !== $previousEvent) {
                                    $headerName = $data;
                                } else {
                                    $headerName .= $data;
                                }
                                break;
                            case HttpParser::EVENT_HEADER_VALUE:
                                if ($event !== $previousEvent) {
                                    $headers[$headerName][] = $data;
                                    $headerNames[strtolower($headerName)] = $headerName;
                                } else {
                                    $headers[$headerName][count($headers[$headerName]) - 1] .= $data;
                                }
                                break;
                            case HttpParser::EVENT_URL:
                            case HttpParser::EVENT_STATUS:
                                if ($event !== $previousEvent) {
                                    $uriOrReasonPhrase = $data;
                                } else {
                                    $uriOrReasonPhrase .= $data;
                                }
                                break;
                            case HttpParser::EVENT_HEADERS_COMPLETE:
                                $headersCompleted = true;
                                $shouldKeepAlive = $parser->shouldKeepAlive();
                                if ($parser->isChunked()) {
                                    $isChunked = true;
                                    if (!$isServerRequest && $this->streamingChunkedResponse) {
                                        // 流式模式下在 headers 完成后即返回 body stream，
                                        // 后续协议推进由 stream 按需驱动。
                                        $state = new ChunkedBodyState(
                                            buffer: $buffer,
                                            parser: $parser,
                                            parsedOffset: $parsedOffset,
                                            bodyBuffer: new Buffer(Buffer::COMMON_SIZE),
                                        );
                                        $this->activeChunkedBody = $state;
                                        $body = new ChunkedBodyStream(
                                            state: $state,
                                            fillToCallback: function (int $targetLength) use ($state, $timeout): void {
                                                $this->pumpChunkedBodyStateToLength($state, $targetLength, $timeout);
                                            },
                                            fillStreamingCallback: function (int $targetLength) use ($state, $timeout): void {
                                                $this->pumpChunkedBodyStateToLength($state, $targetLength, $timeout, streaming: true);
                                            },
                                            fillAllCallback: function () use ($state, $timeout): void {
                                                $this->pumpChunkedBodyStateToCompletion($state, $timeout);
                                            },
                                            closeCallback: function () use ($state): void {
                                                $this->handleChunkedBodyStreamClosed($state);
                                            },
                                        );
                                        $isStreamingChunkedBody = true;
                                        break 3;
                                    }
                                } else {
                                    $contentLength = $parser->getContentLength();
                                    if ($contentLength > $maxContentLength) {
                                        throw new ProtocolException(HttpStatus::REQUEST_ENTITY_TOO_LARGE);
                                    }
                                }
                                if ($parser->isMultipart()) {
                                    $isMultipart = true;
                                    if ($this->preserveBodyData) {
                                        $body = new Buffer($contentLength);
                                        $unparsedLength = $buffer->getLength() - $parsedOffset;
                                        if ($contentLength < $parsedLength) {
                                            $body->append($buffer, $parsedOffset, $contentLength);
                                            $parsedOffset += $contentLength;
                                        } else {
                                            $body->append($buffer, $parsedOffset, $unparsedLength);
                                            $parsedOffset += $unparsedLength;
                                            $neededLength = $contentLength - $unparsedLength;
                                            if ($neededLength > 0) {
                                                $this->read($body, $unparsedLength, $neededLength);
                                            }
                                        }
                                        /* Notice: There may be some risks associated with doing so,
                                         * but it's the easiest way...
                                         * $parsedOffset is for $body instead of $thisBuffer from now. */
                                        $thisBufferParsedOffset = $parsedOffset;
                                        $buffer = $body;
                                        $parsedOffset = 0;
                                    }
                                }
                                break;
                        }
                    } elseif ($isMultipart) {
                        switch ($event) {
                            case HttpParser::EVENT_MULTIPART_HEADER_FIELD:
                                if ($event !== $previousEvent) {
                                    $multipartHeaderName = strtolower($data);
                                } else {
                                    $multipartHeaderName .= strtolower($data);
                                }
                                break;
                            case HttpParser::EVENT_MULTIPART_HEADER_VALUE:
                                if ($event !== $previousEvent) {
                                    $multipartHeaders[$multipartHeaderName] = $data;
                                } else {
                                    $multipartHeaders[$multipartHeaderName] .= $data;
                                }
                                break;
                            case HttpParser::EVENT_MULTIPART_HEADERS_COMPLETE:
                                $multiPartHeadersCompleted = true;
                                /* parse Content-Disposition */
                                $contentDisposition = $multipartHeaders['content-disposition'] ?? '';
                                $contentDispositionParts = explode(';', $contentDisposition, 2);
                                $contentDispositionType = $contentDispositionParts[0];
                                // FIXME: is inline/attachment valid?
                                if (!in_array($contentDispositionType, ['form-data', 'inline', 'attachment'], true)) {
                                    throw new ProtocolException(HttpStatus::BAD_REQUEST, "Unsupported Content-Disposition type '{$contentDispositionParts[0]}'");
                                }
                                $contentDispositionParts = explode(';', $contentDispositionParts[1] ?? '');
                                $contentDispositionMap = [];
                                foreach ($contentDispositionParts as $contentDispositionPart) {
                                    $contentDispositionKeyValue = explode('=', $contentDispositionPart, 2);
                                    $contentDispositionMap[trim($contentDispositionKeyValue[0], ' ')] = trim($contentDispositionKeyValue[1] ?? '', ' "');
                                }
                                $formDataName = $contentDispositionMap['name'] ?? null;
                                $fileName = $contentDispositionMap['filename'] ?? null;
                                if (!$fileName && !$formDataName) {
                                    throw new ProtocolException(HttpStatus::BAD_REQUEST, 'Missing name or filename in Content-Disposition');
                                }

                                if ($fileName) {
                                    // TODO: make dir and prefix configurable
                                    $tmpName = tempnam(sys_get_temp_dir(), 'swow_uploaded_file_');
                                    $tmpFile = fopen($tmpName, 'rwb+');
                                } else {
                                    // TODO: not hard code here?
                                    $formDataValue = new Buffer(256);
                                }
                                break;
                            case HttpParser::EVENT_MULTIPART_BODY:
                                if (isset($formDataValue)) {
                                    $formDataValue->append($buffer, $dataOffset, $dataLength);
                                } else {
                                    if ($fileError !== UPLOAD_ERR_OK) {
                                        break;
                                    }
                                    if ($dataOffset === 0) {
                                        $nWrite = fwrite($tmpFile, $buffer->toString(), $dataLength);
                                    } else {
                                        // TODO: maybe we should have something like fwrite_ex() to gain more performance?
                                        $nWrite = fwrite($tmpFile, $buffer->read($dataOffset, $dataLength));
                                    }
                                    if ($nWrite !== $dataLength) {
                                        $fileError = UPLOAD_ERR_CANT_WRITE;
                                    } else {
                                        $tmpFileSize += $nWrite;
                                    }
                                }
                                break;
                            case HttpParser::EVENT_MULTIPART_DATA_END:
                                if (isset($formDataValue)) {
                                    $formData[$formDataName] = $formDataValue->toString();
                                    // reset for the next parts
                                    $formDataName = '';
                                    $formDataValue = null;
                                } else {
                                    $uploadedFile = new UploadedFileEntity();
                                    $uploadedFile->name = $fileName;
                                    $uploadedFile->type = $multipartHeaders['content-type'] ?? MimeType::TXT;
                                    $uploadedFile->tmpName = $tmpName;
                                    $uploadedFile->tmpFile = $tmpFile;
                                    $uploadedFile->error = $fileError;
                                    $uploadedFile->size = $tmpFileSize;
                                    $uploadedFiles[$formDataName] = $uploadedFile;
                                    // reset for the next parts
                                    $tmpName = '';
                                    $tmpFile = null;
                                    $tmpFileSize = 0;
                                    $fileError = UPLOAD_ERR_OK;
                                }
                                // reset for the next parts
                                $multiPartHeadersCompleted = false;
                                $multipartHeaderName = '';
                                $multipartHeaders = [];
                        }
                    } else {
                        switch ($event) {
                            case HttpParser::EVENT_BODY:
                                if ($isChunked) {
                                    ($body ??= new Buffer(Buffer::COMMON_SIZE))->append($buffer, $dataOffset, $dataLength);
                                    $neededLength = $currentChunkLength - $dataLength;
                                    if ($neededLength > 0) {
                                        $bodyParsedOffset = $body->getLength();
                                        if ($body->getAvailableSize() < $neededLength) {
                                            $body->extend($bodyParsedOffset + $neededLength);
                                        }
                                        $this->read($body, $bodyParsedOffset, $neededLength);
                                        $bodyParsedOffset += $parser->execute($body, $bodyParsedOffset);
                                        if ($parser->getEvent() !== HttpParser::EVENT_BODY) {
                                            throw new ParserException(sprintf(
                                                'Expected EVENT_BODY for chunked message, got %s',
                                                $parser->getEventName()
                                            ));
                                        }
                                        if ($bodyParsedOffset !== $body->getLength()) {
                                            throw new ParserException(sprintf(
                                                'Expected all data of chunked body was parsed, but got %d/%d',
                                                $bodyParsedOffset, $body->getLength()
                                            ));
                                        }
                                    }
                                    break;
                                }
                                $body = new Buffer($contentLength);
                                $body->append($buffer, $dataOffset, $dataLength);
                                $neededLength = $contentLength - $dataLength;
                                $bodyParsedOffset = $dataLength;
                                if ($neededLength > 0) {
                                    if ($dataOffset + $dataLength !== $buffer->getLength()) {
                                        throw new ParserException(sprintf(
                                            'Expected all data has been parsed for body, got %d remaining bytes',
                                            $buffer->getLength() - $bodyParsedOffset
                                        ));
                                    }
                                    /* receive all body data at once here (for performance) */
                                    $this->read($body, $bodyParsedOffset, $neededLength);
                                    $bodyParsedOffset += $parser->execute($body, $bodyParsedOffset);
                                    if ($parser->getEvent() !== HttpParser::EVENT_BODY) {
                                        throw new ParserException(sprintf('Expected EVENT_BODY, got %s', $parser->getEventName()));
                                    }
                                    if ($bodyParsedOffset !== $body->getLength()) {
                                        throw new ParserException(sprintf(
                                            'Expected all data of body was parsed, but got %d/%d',
                                            $bodyParsedOffset, $body->getLength()
                                        ));
                                    }
                                }
                                /* execute again to trigger MESSAGE_COMPLETE event */
                                $parsedLength = $parser->execute($body, $bodyParsedOffset);
                                if ($parsedLength !== 0) {
                                    throw new ParserException('Expected 0 parsed length for MESSAGE_COMPLETE, got %d', $parsedLength);
                                }
                                $event = $parser->getEvent();
                                if ($event !== HttpParser::EVENT_MESSAGE_COMPLETE) {
                                    throw new ParserException(sprintf('Expected MESSAGE_COMPLETE, got %s', $parser->getEventName()));
                                }
                                break 3; /* end */
                            case HttpParser::EVENT_CHUNK_HEADER:
                                $currentChunkLength = $parser->getCurrentChunkLength();
                                $contentLength += $currentChunkLength;
                                if ($contentLength > $maxContentLength) {
                                    throw new ProtocolException(HttpStatus::REQUEST_ENTITY_TOO_LARGE);
                                }
                                break;
                            case HttpParser::EVENT_CHUNK_COMPLETE:
                                if ($currentChunkLength === 0) {
                                    $parsedOffset += $parser->execute($buffer, $parsedOffset);
                                    if ($parser->getEvent() !== HttpParser::EVENT_MESSAGE_COMPLETE) {
                                        throw new ParserException(sprintf(
                                            'Expected MESSAGE_COMPLETE for chunked message, got %s',
                                            $parser->getEventName()
                                        ));
                                    }
                                    break 3; /* end */
                                }
                                break;
                            default:
                                throw new ParserException("Unexpected HttpParser event: {$parser->getEventName()}");
                        }
                    }
                }
            }
        } catch (ParserException $parserException) {
            /* Note: Connection should be reset, it's an unrecoverable error. */
            $shouldKeepAlive = false;
            throw new ProtocolException(HttpStatus::BAD_REQUEST, 'Protocol Parsing Error', $parserException);
        } catch (SocketException $socketException) {
            $shouldKeepAlive = false;
            try {
                $parser->finish();
            } catch (ParserException $parserException) {
                // try to finish failed, re-throw socket exception
                ExceptionEditor::setMessage(
                    $socketException,
                    sprintf(
                        '%s, and parser finish failed with %s',
                        $socketException->getMessage(), $parserException->getMessage()
                    )
                );
                throw $socketException;
            }
            if (!$headersCompleted) {
                // headers are incomplete, re-throw socket exception
                ExceptionEditor::setMessage(
                    $socketException,
                    sprintf(
                        '%s, and headers are incomplete',
                        $socketException->getMessage()
                    )
                );
                throw $socketException;
            }
        } finally {
            if ($isServerRequest) {
                $messageEntity->method = $parser->getMethod();
                $messageEntity->uri = $uriOrReasonPhrase;
                $messageEntity->isUpgrade = $parser->isUpgrade();
                $messageEntity->isMultipart = $isMultipart;
                if (isset($headerNames['cookie'])) {
                    parse_str(
                        strtr(
                            implode(', ', $headers[$headerNames['cookie']]),
                            ['&' => '%26', '+' => '%2B', ';' => '&']
                        ),
                        $messageEntity->cookies
                    );
                }
                $messageEntity->formData = $formData;
                $messageEntity->uploadedFiles = $uploadedFiles;
                $messageEntity->serverParams = $this->getServerParams();
            } else {
                $messageEntity->statusCode = $parser->getStatusCode();
                $messageEntity->reasonPhrase = $uriOrReasonPhrase;
            }
            $messageEntity->headers = $headers;
            $messageEntity->body = $body;
            $messageEntity->protocolVersion = $parser->getProtocolVersion();
            $messageEntity->headerNames = $headerNames;
            $messageEntity->contentLength = $contentLength;
            $messageEntity->shouldKeepAlive = $shouldKeepAlive;
            $this->shouldKeepAlive = $shouldKeepAlive;
        }
        if (!$isStreamingChunkedBody) {
            $parser->reset();
            $this->updateParsedOffsetAndRecycleBufferSpace($thisBuffer, $thisBufferParsedOffset ?? $parsedOffset);
        }

        return $messageEntity;
    }

    /**
     * 在解析下一条 HTTP 消息前，先处理上一条未完成的流式 chunked body。
     *
     * 触发时机：
     * - recvMessageEntity() 入口第一步。
     *
     * 副作用：
     * - 发现未完成 body 时直接关闭连接并将 shouldKeepAlive 置为 false。
     */
    protected function guardPreviousChunkedBody(): void
    {
        $state = $this->activeChunkedBody;
        if ($state === null) {
            return;
        }
        if ($state->finalized) {
            $this->activeChunkedBody = null;
            return;
        }
        $this->activeChunkedBody = null;
        $this->shouldKeepAlive = false;
        // 该连接上出现未完成 body，直接断开，避免后续请求读到残留字节。
        $this->close();
        throw new ProtocolException(
            HttpStatus::BAD_REQUEST,
            sprintf(
                'Previous streaming chunked body was not fully consumed (buffered_bytes=%d, complete=%s)',
                $state->bodyBuffer->getLength(),
                $state->complete ? 'true' : 'false'
            )
        );
    }

    /**
     * 处理业务层主动 close() 的流式 body。
     *
     * 设计意图：
     * - 业务可能只读取了前缀数据就关闭 body；
     * - 当前策略固定为“未完成即断开连接”，简化维护并避免状态分叉。
     */
    protected function handleChunkedBodyStreamClosed(ChunkedBodyState $state): void
    {
        if ($state->finalized) {
            return;
        }
        $this->activeChunkedBody = null;
        $this->shouldKeepAlive = false;
        try {
            $this->close();
        } catch (Throwable) {
            // ignore
        }
    }

    /**
     * 一直推进直到 MESSAGE_COMPLETE（用于 getSize/toString/getContents 等全量语义）。
     */
    protected function pumpChunkedBodyStateToCompletion(ChunkedBodyState $state, ?int $timeout = null): void
    {
        $this->pumpChunkedBodyStateToLength($state, PHP_INT_MAX, $timeout);
    }

    /**
     * 核心推进器：按需把 bodyBuffer 推进到 targetLength，或推进到 finalized。
     *
     * @param bool $streaming 流式模式：每收到一批 body 数据就返回，不等凑满 targetLength。
     *                        供 ChunkedBodyStream::read() 使用，保证 SSE 等场景的低延迟。
     *                        非流式模式下（seek / fillAll）循环到 bodyBuffer >= targetLength。
     */
    protected function pumpChunkedBodyStateToLength(ChunkedBodyState $state, int $targetLength, ?int $timeout = null, bool $streaming = false): void
    {
        if ($state->finalized || $state->bodyBuffer->getLength() >= $targetLength) {
            return;
        }
        $buffer = $state->buffer;
        $parser = $state->parser;
        $parsedOffset = $state->parsedOffset;
        $maxContentLength = $this->getMaxContentLength();
        $expectMoreData = $parsedOffset === $buffer->getLength();
        $timeout ??= $this->recvMessageTimeout;
        $readTimeout = $this->getReadTimeout();
        if (($timeout < 0 ? PHP_INT_MAX : $timeout) < ($readTimeout < 0 ? PHP_INT_MAX : $readTimeout)) {
            $readTimeout = $timeout;
        }
        $mainCoroutine = Coroutine::getMain();
        $bodyLengthOnEntry = $streaming ? $state->bodyBuffer->getLength() : -1;
        try {
            while (!$state->finalized && $state->bodyBuffer->getLength() < $targetLength) {
                // mid-chunk 增量读取：当前 chunk 还有未读字节时，用 recvData 非精确读
                // 直接写入 bodyBuffer，有多少收多少——避免大 chunk 阻塞流式传输。
                if ($state->remainingChunkBytes > 0) {
                    $bodyReadOffset = $state->bodyBuffer->getLength();
                    $maxRecv = min($state->remainingChunkBytes, $targetLength - $bodyReadOffset);
                    if ($maxRecv <= 0) {
                        break;
                    }
                    if ($state->bodyBuffer->getAvailableSize() < $maxRecv) {
                        $state->bodyBuffer->extend($bodyReadOffset + $maxRecv);
                    }
                    $recvd = $this->recvData($state->bodyBuffer, offset: $bodyReadOffset, size: $maxRecv, timeout: $readTimeout);
                    $state->remainingChunkBytes -= $recvd;
                    $bodyReadOffset += $parser->execute($state->bodyBuffer, $bodyReadOffset);
                    if ($parser->getEvent() !== HttpParser::EVENT_BODY) {
                        throw new ParserException(sprintf(
                            'Expected EVENT_BODY for remaining chunk data, got %s',
                            $parser->getEventName()
                        ));
                    }
                    if ($bodyReadOffset !== $state->bodyBuffer->getLength()) {
                        throw new ParserException(sprintf(
                            'Expected all remaining chunk data was parsed, but got %d/%d',
                            $bodyReadOffset,
                            $state->bodyBuffer->getLength()
                        ));
                    }
                    if ($streaming) {
                        // 流式：收到数据即跳出，让上层 read() 返回已有字节
                        break;
                    }
                    continue;
                }
                if ($expectMoreData) {
                    // 流式模式下已有新 body 数据时不再阻塞等更多，让上层及时处理
                    if ($streaming && $state->bodyBuffer->getLength() > $bodyLengthOnEntry) {
                        break;
                    }
                    // recvData 的 offset 不能等于 buffer size；当缓冲区已满且已全部解析时先回收。
                    if ($parsedOffset >= $buffer->getLength() && $buffer->isFull()) {
                        $buffer->truncateFrom($parsedOffset);
                        $parsedOffset = 0;
                    }
                    /*
                     * 到这里仍然 full，说明“未解析数据”已经占满整个协议缓冲区，
                     * 且当前循环又判定还需要更多数据才能继续解析。
                     * 这通常意味着协议片段异常过长或解析状态无进展，再继续收包也没有可写空间，
                     * 必须立刻失败，避免死循环或写越界。
                     */
                    if ($buffer->isFull()) {
                        throw new ParserException('Buffer is full and unable to continue receiving chunked stream');
                    }
                    if ($timeout >= 0) {
                        if (!isset($startTime)) {
                            $startTime = $mainCoroutine->getElapsed();
                        } else {
                            $timePassed = $mainCoroutine->getElapsed() - $startTime;
                            if ($timePassed > $timeout) {
                                throw new SocketException(sprintf('Recv HTTP chunked body timeout (expect within %d ms, but %d ms passed)', $timeout, $timePassed), Errno::ETIMEDOUT);
                            }
                            $readTimeout = $timeout - $timePassed;
                        }
                    }
                    $this->recvData($buffer, offset: $buffer->getLength(), timeout: $readTimeout);
                    $expectMoreData = false;
                }
                $parsedLength = $parser->execute($buffer, $parsedOffset);
                $parsedOffset += $parsedLength;
                $event = $parser->getEvent();
                if ($event === HttpParser::EVENT_NONE) {
                    // 与主解析路径保持一致：NONE 表示需要更多数据，先回收已解析区间再继续收包。
                    $buffer->truncateFrom($parsedOffset);
                    if ($buffer->isFull()) {
                        throw new ParserException('Buffer is full and unable to continue parsing chunked stream');
                    }
                    $parsedOffset = 0;
                    $expectMoreData = true;
                    continue;
                }
                switch ($event) {
                    case HttpParser::EVENT_CHUNK_HEADER:
                        $state->currentChunkLength = $parser->getCurrentChunkLength();
                        if ($state->bodyBuffer->getLength() + $state->currentChunkLength > $maxContentLength) {
                            throw new ProtocolException(HttpStatus::REQUEST_ENTITY_TOO_LARGE);
                        }
                        break;
                    case HttpParser::EVENT_BODY:
                        $dataOffset = $parser->getDataOffset();
                        $dataLength = $parser->getDataLength();
                        $state->bodyBuffer->append($buffer, $dataOffset, $dataLength);
                        // chunk 剩余未在协议缓冲区中的字节，记入 state 由 mid-chunk 路径增量拉取
                        $state->remainingChunkBytes = $state->currentChunkLength - $dataLength;
                        break;
                    case HttpParser::EVENT_CHUNK_COMPLETE:
                        if ($state->currentChunkLength === 0) {
                            $parsedOffset += $parser->execute($buffer, $parsedOffset);
                            if ($parser->getEvent() !== HttpParser::EVENT_MESSAGE_COMPLETE) {
                                throw new ParserException(sprintf(
                                    'Expected MESSAGE_COMPLETE for chunked stream, got %s',
                                    $parser->getEventName()
                                ));
                            }
                            $event = HttpParser::EVENT_MESSAGE_COMPLETE;
                        } else {
                            break;
                        }
                        // no break
                    case HttpParser::EVENT_MESSAGE_COMPLETE:
                        $state->complete = true;
                        $state->parsedOffset = $parsedOffset;
                        // 只在 MESSAGE_COMPLETE 时 finalize，保证 trailer 等协议尾部已消费完毕。
                        $this->finalizeChunkedBodyState($state);
                        return;
                    default:
                        throw new ParserException(sprintf(
                            'Unexpected HttpParser event for chunked stream: %s',
                            $parser->getEventName()
                        ));
                }
            }
        } catch (ParserException $parserException) {
            $this->shouldKeepAlive = false;
            $this->close();
            throw new ProtocolException(HttpStatus::BAD_REQUEST, 'Protocol Parsing Error', $parserException);
        } catch (SocketException $socketException) {
            $this->shouldKeepAlive = false;
            ExceptionEditor::setMessage(
                $socketException,
                sprintf(
                    '%s [streaming-chunked-context: event=%s, parsed_offset=%d, buffer_length=%d, body_buffered=%d, current_chunk_length=%d]',
                    $socketException->getMessage(),
                    $parser->getEventName(),
                    $parsedOffset,
                    $buffer->getLength(),
                    $state->bodyBuffer->getLength(),
                    $state->currentChunkLength
                )
            );
            throw $socketException;
        } finally {
            $state->parsedOffset = $parsedOffset;
        }
    }

    /**
     * 提交流式 body 的最终状态到连接级上下文。
     *
     * 做三件事：
     * 1) 标记 complete/finalized；
     * 2) reset parser；
     * 3) 按 parsedOffset 回收 buffer 并清空 activeChunkedBody。
     */
    protected function finalizeChunkedBodyState(ChunkedBodyState $state): void
    {
        if ($state->finalized) {
            return;
        }
        $state->complete = true;
        $state->finalized = true;
        // 必须在 finalize 时统一 reset/recycle，确保连接解析状态与缓冲区位置一致。
        $state->parser->reset();
        $this->updateParsedOffsetAndRecycleBufferSpace($state->buffer, $state->parsedOffset);
        if ($this->activeChunkedBody === $state) {
            $this->activeChunkedBody = null;
        }
    }

    /**
     * @note This method will unmask masked payloadData and clear the masking key automatically
     */
    public function recvWebSocketFrameEntity(): WebSocketFrameEntity
    {
        $buffer = $this->buffer;
        $parsedOffset = $this->parsedOffset;
        $unparsedLength = $buffer->getLength() - $parsedOffset;
        $maxContentLength = $this->getMaxContentLength();

        $frame = new WebSocketFrameEntity();
        $header = $frame;
        $payloadData = null;
        try {
            /* recv header */
            while ($unparsedLength < WebSocket::HEADER_MIN_SIZE) {
                $unparsedLength += $this->recvData($buffer, offset: $buffer->getLength());
            }
            $header->write(
                offset: 0,
                string: $buffer,
                start: $parsedOffset,
                length: WebSocket::HEADER_MIN_SIZE
            );
            $headerSize = $header->getHeaderSize();
            while ($unparsedLength < $headerSize) {
                $unparsedLength += $this->recvData($buffer, offset: $buffer->getLength());
            }
            $header->write(
                offset: WebSocket::HEADER_MIN_SIZE,
                string: $buffer,
                start: $parsedOffset + WebSocket::HEADER_MIN_SIZE,
                length: $headerSize - WebSocket::HEADER_MIN_SIZE
            );
            $parsedOffset += $headerSize;
            $unparsedLength -= $headerSize;
            /* recv payload data */
            $payloadLength = $header->getPayloadLength();
            if ($payloadLength > $maxContentLength) {
                throw new ProtocolException(HttpStatus::REQUEST_ENTITY_TOO_LARGE);
            }
            if ($payloadLength > 0) {
                $payloadData = new Buffer($payloadLength);
                if ($unparsedLength >= $payloadLength) {
                    $payloadData->append($buffer, $parsedOffset, $payloadLength);
                } else {
                    $payloadData->append($buffer, $parsedOffset, $unparsedLength);
                    $this->read(
                        buffer: $payloadData,
                        offset: $unparsedLength,
                        length: $payloadLength - $unparsedLength
                    );
                }
                /* Notice: $parsedOffset may be bigger than $buffer->getLength(),
                 * because we may recv payloadData from remote instead of unparsed buffer. */
                $parsedOffset += $payloadLength;
                if ($header->getMask() && $this->autoUnmask) {
                    WebSocket::unmask($payloadData, maskingKey: $header->getMaskingKey());
                    $header->setMaskingKey(''); // drop mask and masking key
                }
            }
        } finally {
            $frame->payloadData = $payloadData;
        } /* TODO: with bad message */
        /* recv and parsed done */
        $this->updateParsedOffsetAndRecycleBufferSpace($buffer, $parsedOffset);

        return $frame;
    }

    protected function updateParsedOffsetAndRecycleBufferSpace(Buffer $buffer, int $parsedOffset): void
    {
        if (
            /* All data has been parsed, clear them
             * Notice: do not use $parsedOffset === $buffer->getLength() here,
             * because $parsedOffset may be bigger than $buffer->getLength() when
             * we recv payloadData from remote instead of unparsed buffer. */
            $parsedOffset >= $buffer->getLength() ||
            /* More than size * load_factor of the data has been parsed,
             * prefer to move the remaining data to the front of buffer.
             * Otherwise, the length of data received next time may be a little less,
             * this will result in more socket reads. */
            $parsedOffset > ($buffer->getSize() * $this->bufferLoadFactor)
        ) {
            $buffer->truncateFrom($parsedOffset);
            $this->parsedOffset = 0;
        } else {
            $this->parsedOffset = $parsedOffset;
        }
    }
}
