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

use Swow\Buffer;
use Swow\Http\Message\ResponseEntity;
use Swow\Http\Message\ServerRequestEntity;
use Swow\Http\Message\UploadedFileEntity;
use Swow\Http\Message\WebSocketFrameEntity;
use Swow\Http\Parser as HttpParser;
use Swow\Http\Status as HttpStatus;
use Swow\Psr7\Message\ResponseException;
use Swow\WebSocket\WebSocket;
use ValueError;

use function array_filter;
use function array_map;
use function count;
use function explode;
use function fopen;
use function fwrite;
use function implode;
use function in_array;
use function parse_str;
use function sprintf;
use function strcasecmp;
use function strtolower;
use function sys_get_temp_dir;
use function tempnam;
use function trim;

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

    protected int $maxBufferSize = Buffer::COMMON_SIZE;

    protected float $bufferLoadFactor = 0.75;

    protected bool $preserveBodyData = false;

    protected ?bool $shouldKeepAlive = false;

    protected function __constructReceiver(int $type, int $events): void
    {
        $this->buffer = new Buffer(Buffer::COMMON_SIZE);
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

    public function getMaxBufferSize(): int
    {
        return $this->maxBufferSize;
    }

    public function setMaxBufferSize(int $maxBufferSize): static
    {
        if (($maxBufferSize % Buffer::PAGE_SIZE) !== 0) {
            throw new ValueError(sprintf(
                '%s(): Argument#1 ($maxBufferSize) should be multiple of memory page size (%d)',
                __METHOD__, Buffer::PAGE_SIZE
            ));
        }
        $this->maxBufferSize = $maxBufferSize;

        return $this;
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

    public function setPreserveBodyData(bool $value): static
    {
        $this->preserveBodyData = $value;

        return $this;
    }

    public function shouldKeepAlive(): ?bool
    {
        return $this->shouldKeepAlive;
    }

    /**
     * @TODO The options must be managed in a unified way
     * @phan-return T
     * @phpstan-return T
     * @psalm-return T
     */
    protected function recvMessageEntity(): ServerRequestEntity|ResponseEntity
    {
        $buffer = $this->buffer;
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
        /** @var array<string, array<string>> */
        $headers = [];
        /** @var array<string, string> */
        $headerNames = [];
        $shouldKeepAlive = false;
        $contentLength = 0;
        $headerLength = 0;
        $headersComplete = false;
        $isChunked = false;
        $currentChunkLength = 0;
        $body = null;
        /* }}} */
        /* multipart related values {{{ */
        $isMultipart = false;
        $multiPartHeadersComplete = false;
        $multipartHeaderName = '';
        $multipartHeaders = [];
        $tmpName = '';
        $tmpFile = null;
        $tmpFileSize = 0;
        $fileError = UPLOAD_ERR_OK;
        $formData = [];
        $uploadedFiles = [];
        /* }}} */
        try {
            while (true) {
                if ($expectMoreData) {
                    $this->recvData($buffer, $buffer->getLength());
                    $expectMoreData = false;
                }
                // TODO: call $parser->finished() if connection error?
                while (true) {
                    $previousEvent = $event;
                    $parsedLength = $parser->execute($buffer->toString(), $parsedOffset);
                    $parsedOffset += $parsedLength;
                    $event = $parser->getEvent();
                    if ($event & HttpParser::EVENT_FLAG_DATA) {
                        $dataOffset = $parser->getDataOffset();
                        $dataLength = $parser->getDataLength();
                        if (!$headersComplete || ($isMultipart && !$multiPartHeadersComplete)) {
                            $data = $buffer->read($dataOffset, $dataLength);
                        }
                        $expectMoreData = true;
                    }
                    if (!$headersComplete) {
                        $headerLength += $parsedLength;
                        if ($headerLength > $maxHeaderLength) {
                            throw new ResponseException(empty($headerName) ? HttpStatus::REQUEST_URI_TOO_LARGE : HttpStatus::REQUEST_HEADER_FIELDS_TOO_LARGE);
                        }
                    }
                    if ($event === HttpParser::EVENT_NONE) {
                        $buffer->truncateFrom($parsedOffset);
                        if ($buffer->isFull()) {
                            throw new ParserException('Buffer is full and unable to continue parsing');
                        }
                        $parsedOffset = 0;
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
                                })
                            );
                            $headers[$headerNames['content-length'] ??= 'Content-Length'] = $body ? (string) $body->getLength() : '0';
                            if ($body && $body->getLength() < ($body->getSize() / 2)) {
                                $body->mallocTrim();
                            }
                        } elseif ($isMultipart && !$this->preserveBodyData) {
                            $contentLengthIndex = $headerNames['content-length'] ?? null;
                            if ($contentLengthIndex) {
                                unset($headers[$contentLengthIndex]);
                            }
                        }
                        break 2;
                    }
                    if (!$headersComplete) {
                        switch ($event) {
                            case HttpParser::EVENT_HEADER_FIELD:
                                {
                                    if ($event !== $previousEvent) {
                                        $headerName = $data;
                                    } else {
                                        $headerName .= $data;
                                    }
                                    break;
                                }
                            case HttpParser::EVENT_HEADER_VALUE:
                                {
                                    if ($event !== $previousEvent) {
                                        $headers[$headerName][] = $data;
                                        $headerNames[strtolower($headerName)] = $headerName;
                                    } else {
                                        $headers[$headerName][count($headers[$headerName]) - 1] .= $data;
                                    }
                                    break;
                                }
                            case HttpParser::EVENT_URL:
                            case HttpParser::EVENT_STATUS:
                                {
                                    if ($event !== $previousEvent) {
                                        $uriOrReasonPhrase = $data;
                                    } else {
                                        $uriOrReasonPhrase .= $data;
                                    }
                                    break;
                                }
                            case HttpParser::EVENT_HEADERS_COMPLETE:
                                {
                                    $defaultKeepAlive = $parser->getMajorVersion() !== 1 || $parser->getMinorVersion() !== 0;
                                    $shouldKeepAlive = $parser->shouldKeepAlive();
                                    $this->shouldKeepAlive = $shouldKeepAlive !== $defaultKeepAlive ? $shouldKeepAlive : null;
                                    if ($parser->isChunked()) {
                                        $isChunked = true;
                                    } else {
                                        $contentLength = $parser->getContentLength();
                                        if ($contentLength > $maxContentLength) {
                                            throw new ResponseException(HttpStatus::REQUEST_ENTITY_TOO_LARGE);
                                        }
                                    }
                                    $headersComplete = true;
                                    if ($parser->isMultipart()) {
                                        $isMultipart = true;
                                        if ($this->preserveBodyData) {
                                            $body = new Buffer($contentLength);
                                            $unparsedLength = $buffer->getLength() - $parsedOffset;
                                            if ($contentLength < $parsedLength) {
                                                $body->append($buffer->toString(), $parsedOffset, $contentLength);
                                            } else {
                                                $body->append($buffer->toString(), $parsedOffset, $unparsedLength);
                                                $neededLength = $contentLength - $unparsedLength;
                                                if ($neededLength > 0) {
                                                    $this->read($body, $unparsedLength, $neededLength);
                                                }
                                            }
                                            // Notice: There may be some risks associated with doing so,
                                            // but it's the easiest way...
                                            $buffer = $body;
                                            $parsedOffset = 0;
                                        }
                                    }
                                    break;
                                }
                        }
                    } elseif ($isMultipart) {
                        switch ($event) {
                            case HttpParser::EVENT_MULTIPART_HEADER_FIELD:
                                {
                                    if ($event !== $previousEvent) {
                                        $multipartHeaderName = strtolower($data);
                                    } else {
                                        $multipartHeaderName .= strtolower($data);
                                    }
                                    break;
                                }
                            case HttpParser::EVENT_MULTIPART_HEADER_VALUE:
                                {
                                    if ($event !== $previousEvent) {
                                        $multipartHeaders[$multipartHeaderName] = $data;
                                    } else {
                                        $multipartHeaders[$multipartHeaderName] .= $data;
                                    }
                                    break;
                                }
                            case HttpParser::EVENT_MULTIPART_HEADERS_COMPLETE:
                                {
                                    $multiPartHeadersComplete = true;
                                    /* parse Content-Disposition */
                                    $contentDisposition = $multipartHeaders['content-disposition'] ?? '';
                                    $contentDispositionParts = explode(';', $contentDisposition, 2);
                                    $contentDispositionType = $contentDispositionParts[0];
                                    // FIXME: is inline/attachment valid?
                                    if (!in_array($contentDispositionType, ['form-data', 'inline', 'attachment'], true)) {
                                        throw new ResponseException(HttpStatus::BAD_REQUEST, "Unsupported Content-Disposition type '{$contentDispositionParts[0]}'");
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
                                        throw new ResponseException(HttpStatus::BAD_REQUEST, 'Missing name or filename in Content-Disposition');
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
                                }
                            case HttpParser::EVENT_MULTIPART_BODY:
                                {
                                    if (isset($formDataValue)) {
                                        $formDataValue->append($buffer->toString(), $dataOffset, $dataLength);
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
                                }
                            case HttpParser::EVENT_MULTIPART_DATA_END:
                                {
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
                                    $multiPartHeadersComplete = false;
                                    $multipartHeaderName = '';
                                    $multipartHeaders = [];
                                }
                        }
                    } else {
                        switch ($event) {
                            case HttpParser::EVENT_BODY:
                                {
                                    if ($isChunked) {
                                        ($body ??= new Buffer(Buffer::COMMON_SIZE))->append($buffer->toString(), $dataOffset, $dataLength);
                                        $neededLength = $currentChunkLength - $dataLength;
                                        if ($neededLength > 0) {
                                            $bodyParsedOffset = $body->getLength();
                                            if ($body->getAvailableSize() < $neededLength) {
                                                $body->extend($bodyParsedOffset + $neededLength);
                                            }
                                            $this->read($body, $bodyParsedOffset, $neededLength);
                                            $bodyParsedOffset += $parser->execute($body->toString(), $bodyParsedOffset);
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
                                    $body->append($buffer->toString(), $dataOffset, $dataLength);
                                    $neededLength = $contentLength - $dataLength;
                                    $bodyParsedOffset = $dataLength;
                                    if ($neededLength > 0) {
                                        if ($bodyParsedOffset !== $buffer->getLength()) {
                                            throw new ParserException(sprintf(
                                                'Expected all data has been parsed for body, got %d remaining bytes',
                                                $buffer->getLength() - $bodyParsedOffset
                                            ));
                                        }
                                        /* receive all body data at once here (for performance) */
                                        $this->read($body, $bodyParsedOffset, $neededLength);
                                        $bodyParsedOffset += $parser->execute($body->toString(), $bodyParsedOffset);
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
                                    $parsedLength = $parser->execute($body->toString(), $bodyParsedOffset);
                                    if ($parsedLength !== 0) {
                                        throw new ParserException('Expected 0 parsed length for MESSAGE_COMPLETE, got %d', $parsedLength);
                                    }
                                    $event = $parser->getEvent();
                                    if ($event !== HttpParser::EVENT_MESSAGE_COMPLETE) {
                                        throw new ParserException(sprintf('Expected MESSAGE_COMPLETE, got %s', $parser->getEventName()));
                                    }
                                    break 3; /* end */
                                }
                            case HttpParser::EVENT_CHUNK_HEADER:
                                {
                                    $currentChunkLength = $parser->getCurrentChunkLength();
                                    $contentLength += $currentChunkLength;
                                    if ($contentLength > $maxContentLength) {
                                        throw new ResponseException(HttpStatus::REQUEST_ENTITY_TOO_LARGE);
                                    }
                                    break;
                                }
                            case HttpParser::EVENT_CHUNK_COMPLETE:
                                {
                                    if ($currentChunkLength === 0) {
                                        $parsedOffset += $parser->execute($buffer->toString(), $parsedOffset);
                                        if ($parser->getEvent() !== HttpParser::EVENT_MESSAGE_COMPLETE) {
                                            throw new ParserException(sprintf(
                                                'Expected MESSAGE_COMPLETE for chunked message, got %s',
                                                $parser->getEventName()
                                            ));
                                        }
                                        break 3; /* end */
                                    }
                                    break;
                                }
                            default:
                                throw new ParserException("Unexpected HttpParser event: {$parser->getEventName()}");
                        }
                    }
                }
            }
        } catch (ParserException $parserException) {
            /* TODO: Get bad request */
            /* FIXME: Just throw it without ResponseException? Connection should be reset, it's an unrecoverable error. */
            throw new ResponseException(HttpStatus::BAD_REQUEST, 'Protocol Parsing Error', $parserException);
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
        }
        $parser->reset();
        $this->updateParsedOffsetAndRecycleBufferSpace($buffer, $parsedOffset);

        return $messageEntity;
    }

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
                $unparsedLength += $this->recvData($buffer, $buffer->getLength());
            }
            $header->write(
                offset: 0,
                string: $buffer->toString(),
                start: $parsedOffset,
                length: WebSocket::HEADER_MIN_SIZE
            );
            $headerSize = $header->getHeaderSize();
            while ($unparsedLength < $headerSize) {
                $unparsedLength += $this->recvData($buffer, $buffer->getLength());
            }
            $header->write(
                offset: WebSocket::HEADER_MIN_SIZE,
                string: $buffer->toString(),
                start: $parsedOffset + WebSocket::HEADER_MIN_SIZE,
                length: $headerSize - WebSocket::HEADER_MIN_SIZE
            );
            $parsedOffset += $headerSize;
            $unparsedLength -= $headerSize;
            /* recv payload data */
            $payloadLength = $header->getPayloadLength();
            if ($payloadLength > $maxContentLength) {
                throw new ResponseException(HttpStatus::REQUEST_ENTITY_TOO_LARGE); // FIXME
            }
            if ($payloadLength > 0) {
                $payloadData = new Buffer(0);
                if ($unparsedLength >= $payloadLength) {
                    $payloadData->append($buffer->toString(), $parsedOffset, $payloadLength);
                } else {
                    $payloadData->append($buffer->toString(), $parsedOffset, $unparsedLength);
                    $this->read(
                        buffer: $payloadData,
                        offset: $unparsedLength,
                        length: $payloadLength - $unparsedLength
                    );
                }
                $parsedOffset += $payloadLength;
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
            /* All data has been parsed, clear them */
            $buffer->getLength() === $parsedOffset ||
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
