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

use Swow\Http\Exception as HttpException;
use Swow\Http\Parser as HttpParser;
use Swow\Http\Parser\Exception as HttpParserException;
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
        $this->httpParser = (new HttpParser())
            ->setType($type)
            ->setEvents($events);
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
     * TODO: The options must be managed in a unified way
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
        $headers = [];
        $headerNames = [];
        $shouldKeepAlive = false;
        $contentLength = 0;
        $headerLength = 0;
        $headersComplete = false;
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
                            throw new HttpException(HttpStatus::REQUEST_HEADER_FIELDS_TOO_LARGE);
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
                                    throw new HttpException(empty($uriOrReasonPhrase) ? HttpStatus::REQUEST_URI_TOO_LARGE : HttpStatus::REQUEST_HEADER_FIELDS_TOO_LARGE);
                                }
                                $buffer->realloc($newSize);
                            }
                        } else {
                            $buffer->clear();
                        }
                        break;
                    }
                    if ($event === HttpParser::EVENT_MESSAGE_COMPLETE) {
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
                                $contentLength = $parser->getContentLength();
                                if ($contentLength > $maxContentLength) {
                                    throw new HttpException(HttpStatus::REQUEST_ENTITY_TOO_LARGE);
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
                                $multipartHeaderName = strtolower($data);
                                break;
                            case HttpParser::EVENT_MULTIPART_HEADER_VALUE:
                                $multipartHeaders[$multipartHeaderName] = $data;
                                break;
                            case HttpParser::EVENT_MULTIPART_HEADERS_COMPLETE:
                            {
                                $multiPartHeadersComplete = true;
                                // TODO: make dir and prefix configurable
                                $tmpFilePath = tempnam(sys_get_temp_dir(), 'swow_');
                                $tmpFile = fopen($tmpFilePath, 'w+');
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
                                $contentDispositionParts = array_map(function (string $s) {
                                    return trim($s, ' ;');
                                }, explode(';', $multipartHeaders['content-disposition'] ?? ''));
                                if (($contentDispositionParts[0] ?? '') !== 'form-data') {
                                    throw new HttpException(HttpStatus::BAD_REQUEST, "Unsupported Content-Disposition type '{$contentDispositionParts[0]}'");
                                }
                                $fileName = null;
                                foreach ($contentDispositionParts as $contentDispositionPart) {
                                    if (str_starts_with($contentDispositionPart, 'filename=')) {
                                        $fileName = trim(substr($contentDispositionPart, strlen('filename=')), '"');
                                    }
                                }
                                if (!$fileName) {
                                    throw new HttpException(HttpStatus::BAD_REQUEST, 'Missing filename in Content-Disposition');
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
                                $body = new Buffer($contentLength);
                                $body->write($buffer->toString(), $parser->getDataOffset(), $parser->getDataLength());
                                $neededLength = $contentLength - $body->getLength();
                                if ($neededLength > 0) {
                                    $this->read($body, $neededLength);
                                }
                                $event = $parser->execute($body);
                                if ($event === HttpParser::EVENT_BODY) {
                                    $event = $parser->execute($body);
                                }
                                if ($event !== HttpParser::EVENT_MESSAGE_COMPLETE) {
                                    throw new HttpParserException('Unexpected body eof');
                                }
                                break 3; /* end */
                            }
                        }
                    }
                }
            }
        } catch (HttpParserException) {
            /* TODO: get bad request */
            throw new HttpException(HttpStatus::BAD_REQUEST, 'Protocol Parsing Error');
        } finally {
            if ($isRequest) {
                $result->method = $parser->getMethod();
                $result->uri = $uriOrReasonPhrase;
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
            $result->isUpgrade = $parser->isUpgrade();
            $result->uploadedFiles = $uploadedFiles;
            $parser->reset();
        }

        return $result;
    }
}
