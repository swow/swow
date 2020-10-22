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

namespace Swow\Http;

use Swow\Http\Exception as HttpException;
use Swow\Http\Parser as HttpParser;
use Swow\Http\Parser\Exception as HttpParserException;
use Swow\Http\Server\Request;
use Swow\Http\Status as HttpStatus;
use Swow\Socket;

/**
 * @mixin Socket
 */
trait ParserTrait
{
    /**
     * @var Buffer
     */
    protected $buffer;

    /**
     * @var HttpParser
     */
    protected $httpParser;

    /**
     * @var int
     */
    protected $maxBufferSize;

    public function __construct(int $type, int $events, int $maxBufferSize = Buffer::DEFAULT_SIZE)
    {
        $this->buffer = new Buffer();
        $this->httpParser = (new HttpParser())
            ->setType($type)
            ->setEvents($events);
        $this->maxBufferSize = $maxBufferSize;
    }

    public function parse(int $maxHeaderLength, int $maxContentLength): array
    {
        $parser = $this->httpParser;
        $buffer = $this->buffer;

        $expectMore = $buffer->eof();
        if ($expectMore) {
            /* all data has been parsed, clear them */
            $buffer->clear();
        }
        $data = '';
        $uri = '';
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
                }
                while (true) {
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
                            $buffer->truncate();
                            if ($buffer->isFull()) {
                                $newSize = $buffer->getSize() * 2;
                                /* we need bigger buffer to handle the large filed (or throw error) */
                                if ($newSize > $this->maxBufferSize) {
                                    throw new HttpException(!isset($uri) ? HttpStatus::REQUEST_URI_TOO_LARGE : HttpStatus::REQUEST_HEADER_FIELDS_TOO_LARGE);
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
                                $headerName = $data;
                                break;
                            }
                            case HttpParser::EVENT_HEADER_VALUE:
                            {
                                $headers[$headerName] = $data;
                                $headerNames[strtolower($headerName)] = $headerName;
                                break;
                            }
                            case HttpParser::EVENT_URL:
                            {
                                $uri = $data;
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
                                $body = new Buffer(0);
                                $writableSize = $body->getWritableSize();
                                if ($writableSize < $contentLength) {
                                    $body->realloc($body->tell() + $writableSize);
                                }
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

            $method = $parser->getMethod();
            $protocolVersion = $parser->getProtocolVersion();
            $isUpgrade = $parser->isUpgrade();
        } catch (HttpParserException $exception) {
            /* TODO: get bad request */
            throw new HttpException(HttpStatus::BAD_REQUEST, 'Protocol Parsing Error');
        } finally {
            $parser->reset();
        }

        return [
            $uri,
            $method,
            $headers,
            $headerNames,
            $body,
            $contentLength,
            $protocolVersion,
            $shouldKeepAlive,
            $isUpgrade,
        ];
    }
}
