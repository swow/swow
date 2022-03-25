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

use Swow\Http\Parser as HttpParser;
use Swow\Http\Status as HttpStatus;
use function array_map;
use function count;
use function explode;
use function fopen;
use function fwrite;
use function str_starts_with;
use function strlen;
use function strtolower;
use function substr;
use function sys_get_temp_dir;
use function tempnam;
use function trim;
use const UPLOAD_ERR_CANT_WRITE;
use const UPLOAD_ERR_OK;

/**
 * http response / request receiver
 *
 * @phan-template T of RawRequest|RawResponse
 * @phpstan-template T of RawRequest|RawResponse
 * @psalm-template T of RawRequest|RawResponse
 */
trait ReceiverTrait
{
    protected Buffer $buffer;

    protected ?Parser $httpParser = null;

    protected int $maxBufferSize = Buffer::DEFAULT_SIZE;

    protected bool $preserveBodyData = false;

    protected ?bool $keepAlive = false;

    protected function __construct(int $type, int $events)
    {
        $this->buffer = new Buffer();
        $requiredEvents =
            HttpParser::EVENT_URL |
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

    public function setMaxBufferSize(mixed $maxBufferSize): static
    {
        $this->maxBufferSize = $maxBufferSize;

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

    /**
     * @todo The options must be managed in a unified way
     * @phan-return T
     * @phpstan-return T
     * @psalm-return T
     */
    protected function execute(int $maxHeaderLength, int $maxContentLength): RawRequest|RawResponse
    {
        $parser = $this->httpParser;
        $buffer = $this->buffer;
        $isRequest = $parser->getType() === Parser::TYPE_REQUEST;
        $result = $isRequest ? new RawRequest() : new RawResponse();

        $expectMore = $buffer->eof();
        if ($expectMore) {
            /* all data has been parsed, clear them */
            $buffer->clear();
        }
        $data = '';
        $uriOrReasonPhrase = '';
        $headerName = '';
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
        /* multipart related values {{{ */
        $isMultipart = false;
        $multiPartHeadersComplete = false;
        $multipartHeaderName = '';
        $multipartHeaders = [];
        $tmpFilePath = '';
        $tmpFile = null;
        $tmpFileSize = 0;
        $fileError = UPLOAD_ERR_OK;
        $uploadedFiles = [];
        /* }}} */
        try {
            while (true) {
                if ($expectMore) {
                    $this->recvData($buffer);
                    // TODO: call $parser->finished() if connection error?
                }
                $event = HttpParser::EVENT_NONE;
                while (true) {
                    $previousEvent = $event;
                    if (!$headersComplete || ($isMultipart && !$multiPartHeadersComplete)) {
                        $event = $parser->execute($buffer, $data);
                    } else {
                        $event = $parser->execute($buffer);
                    }
                    if (!$headersComplete) {
                        $headerLength += $parser->getParsedLength();
                        if ($headerLength > $maxHeaderLength) {
                            throw new ResponseException(HttpStatus::REQUEST_HEADER_FIELDS_TOO_LARGE);
                        }
                    }
                    if ($event === HttpParser::EVENT_NONE) {
                        $expectMore = true;
                        if (!$buffer->eof()) {
                            /* make sure we have moved the left data to the head */
                            $buffer->truncateFrom();
                            if ($buffer->isFull()) {
                                $newSize = $buffer->getSize() * 2;
                                /* we need bigger buffer to handle the large filed (or throw error) */
                                if ($newSize > $this->maxBufferSize) {
                                    throw new ResponseException(empty($uriOrReasonPhrase) ? HttpStatus::REQUEST_URI_TOO_LARGE : HttpStatus::REQUEST_HEADER_FIELDS_TOO_LARGE);
                                }
                                $buffer->realloc($newSize);
                            }
                        } else {
                            $buffer->clear();
                        }
                        break;
                    }
                    if ($event === HttpParser::EVENT_MESSAGE_COMPLETE) {
                        if ($isChunked) {
                            // TODO: Several values can be listed, separated by a comma. (Transfer-Encoding: gzip, chunked)
                            unset($headers[$headerNames['transfer-encoding']], $headerNames['transfer-encoding']);
                            if ($body && $body->getLength() < ($body->getSize() / 2)) {
                                $body->mallocTrim();
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
                                $this->keepAlive = $shouldKeepAlive !== $defaultKeepAlive ? $shouldKeepAlive : null;
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
                                        $body->write($buffer->toString(), $buffer->tell());
                                        $neededLength = $contentLength - $body->getLength();
                                        if ($neededLength > 0) {
                                            $this->read($body, $neededLength);
                                        }
                                        // Notice: There may be some risks associated with doing so,
                                        // but it's the easiest way...
                                        $buffer = $body->rewind();
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
                                // TODO: make dir and prefix configurable
                                $tmpFilePath = tempnam(sys_get_temp_dir(), 'swow_');
                                $tmpFile = fopen($tmpFilePath, 'wb+');
                                break;
                            }
                            case HttpParser::EVENT_MULTIPART_BODY:
                            {
                                if ($fileError === UPLOAD_ERR_OK) {
                                    $dataOffset = $parser->getDataOffset();
                                    $dataLength = $parser->getDataLength();
                                    if ($dataOffset === 0) {
                                        $nWrite = fwrite($tmpFile, $buffer->toString(), $dataLength);
                                    } else {
                                        $nWrite = fwrite($tmpFile, $buffer->peekFrom($dataOffset, $dataLength));
                                    }
                                    if ($nWrite !== strlen($data)) {
                                        $fileError = UPLOAD_ERR_CANT_WRITE;
                                    } else {
                                        $tmpFileSize += $nWrite;
                                    }
                                }
                                break;
                            }
                            case HttpParser::EVENT_MULTIPART_DATA_END:
                            {
                                $contentDispositionParts = array_map(static function (string $s) {
                                    return trim($s, ' ;');
                                }, explode(';', $multipartHeaders['content-disposition'] ?? ''));
                                if (($contentDispositionParts[0] ?? '') !== 'form-data') {
                                    throw new ResponseException(HttpStatus::BAD_REQUEST, "Unsupported Content-Disposition type '{$contentDispositionParts[0]}'");
                                }
                                $fileName = null;
                                foreach ($contentDispositionParts as $contentDispositionPart) {
                                    if (str_starts_with($contentDispositionPart, 'filename=')) {
                                        $fileName = trim(substr($contentDispositionPart, strlen('filename=')), '"');
                                    }
                                }
                                if (!$fileName) {
                                    throw new ResponseException(HttpStatus::BAD_REQUEST, 'Missing filename in Content-Disposition');
                                }
                                $uploadedFile = new RawUploadedFile();
                                $uploadedFile->name = $fileName;
                                $uploadedFile->type = $multipartHeaders['content-type'] ?? MimeType::TXT;
                                $uploadedFile->tmp_name = $tmpFilePath;
                                $uploadedFile->error = $fileError;
                                $uploadedFile->size = $tmpFileSize;
                                $uploadedFiles[] = $uploadedFile;
                                // reset for the next parts
                                $multiPartHeadersComplete = false;
                                $multipartHeaderName = '';
                                $multipartHeaders = [];
                                $tmpFilePath = '';
                                $tmpFile = null;
                                $tmpFileSize = 0;
                                $fileError = UPLOAD_ERR_OK;
                            }
                        }
                    } else {
                        switch ($event) {
                            case HttpParser::EVENT_BODY:
                            {
                                $data = $buffer->toString();
                                $dataOffset = $parser->getDataOffset();
                                $dataLength = $parser->getDataLength();
                                if ($isChunked) {
                                    $currentChunkLength -= $dataLength;
                                    if ($currentChunkLength < 0) {
                                        throw new ParserException('Unexpected body data (chunk overflow)');
                                    }
                                    ($body ??= new Buffer())->write($data, $dataOffset, $dataLength);
                                    break;
                                }
                                $body = new Buffer($contentLength);
                                $body->write($data, $dataOffset, $dataLength);
                                $neededLength = $contentLength - $body->getLength();
                                if ($neededLength > 0) {
                                    $this->read($body, $neededLength);
                                }
                                $event = $parser->execute($body);
                                if ($event === HttpParser::EVENT_BODY) {
                                    $event = $parser->execute($body);
                                }
                                if ($event !== HttpParser::EVENT_MESSAGE_COMPLETE) {
                                    throw new ParserException('Unexpected body eof');
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
                                if ($currentChunkLength !== 0) {
                                    throw new ParserException('Unexpected chunk eof');
                                }
                                break;
                            }
                            default:
                                throw new ParserException('Unexpected Http-Parser event');
                        }
                    }
                }
            }
        } catch (ParserException) {
            /* TODO: get bad request */
            throw new ResponseException(HttpStatus::BAD_REQUEST, 'Protocol Parsing Error');
        } finally {
            if ($isRequest) {
                $result->method = $parser->getMethod();
                $result->uri = $uriOrReasonPhrase;
                $result->isUpgrade = $parser->isUpgrade();
                $result->uploadedFiles = $uploadedFiles;
            } else {
                $result->statusCode = $parser->getStatusCode();
                $result->reasonPhrase = $uriOrReasonPhrase;
            }
            $result->headers = $headers;
            $result->body = $body?->rewind();
            $result->protocolVersion = $parser->getProtocolVersion();
            $result->headerNames = $headerNames;
            $result->contentLength = $contentLength;
            $result->shouldKeepAlive = $shouldKeepAlive;
            $parser->reset();
        }

        return $result;
    }
}
