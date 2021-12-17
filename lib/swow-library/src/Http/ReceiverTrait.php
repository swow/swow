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
use function count;
use function strtolower;

trait ReceiverTrait
{
    protected Buffer $buffer;

    protected ?Parser $httpParser = null;

    protected int $maxBufferSize;

    protected ?bool $keepAlive = false;

    protected function __construct(int $type, int $events, int $maxBufferSize = Buffer::DEFAULT_SIZE)
    {
        $this->buffer = new Buffer();
        $this->httpParser = (new HttpParser())
            ->setType($type)
            ->setEvents($events);
        $this->maxBufferSize = $maxBufferSize;
    }

    /**
     * TODO: The options must be managed in a unified way
     * @return RawRequest|RawResponse|RawResult
     */
    protected function execute(int $maxHeaderLength, int $maxContentLength): RawResult
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
        try {
            while (true) {
                if ($expectMore) {
                    $this->recvData($buffer);
                    // TODO: call $parser->finished() if connection error?
                }
                $event = HttpParser::EVENT_NONE;
                while (true) {
                    $previousEvent = $event;
                    if (!$headersComplete) {
                        $event = $parser->execute($buffer, $data);
                        $headerLength += $parser->getParsedLength();
                        if ($headerLength > $maxHeaderLength) {
                            throw new HttpException(HttpStatus::REQUEST_HEADER_FIELDS_TOO_LARGE);
                        }
                    } else {
                        $event = $parser->execute($buffer);
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
                                break;
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
                                $body->rewind();
                                if ($event !== HttpParser::EVENT_MESSAGE_COMPLETE) {
                                    throw new HttpParserException('Unexpected body eof');
                                }
                                break 3; /* end */
                            }
                        }
                    }
                }
            }

            if ($isRequest) {
                $result->method = $parser->getMethod();
                $result->uri = $uriOrReasonPhrase;
            } else {
                $result->statusCode = $parser->getStatusCode();
                $result->reasonPhrase = $uriOrReasonPhrase;
            }
            $result->headers = $headers;
            $result->body = $body;
            $result->protocolVersion = $parser->getProtocolVersion();
            $result->headerNames = $headerNames;
            $result->contentLength = $contentLength;
            $result->shouldKeepAlive = $shouldKeepAlive;
            $result->isUpgrade = $parser->isUpgrade();
        } catch (HttpParserException) {
            /* TODO: get bad request */
            throw new HttpException(HttpStatus::BAD_REQUEST, 'Protocol Parsing Error');
        } finally {
            $parser->reset();
        }

        return $result;
    }
}
