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

use Psr\Http\Client\ClientExceptionInterface;
use Psr\Http\Client\ClientInterface;
use Psr\Http\Message\RequestInterface;
use Psr\Http\Message\ResponseInterface;
use Swow\Http\Client\ClientException;
use Swow\Http\Client\NetworkException;
use Swow\Http\Parser as HttpParser;
use Swow\Socket;

class Client extends Socket implements ClientInterface
{
    use ReceiverTrait {
        __construct as receiverConstruct;
        execute as receiverExecute;
    }

    /**
     * @var string
     */
    protected $name;

    /**
     * @var int
     */
    protected $port;

    /**
     * @var int
     */
    protected $timeout;

    /**
     * @var int
     */
    protected $maxHeaderLength = 8192;

    /**
     * @var int
     */
    protected $maxContentLength = 8 * 1024 * 1024;

    public function __construct(string $name, int $port = 0, int $timeout = null)
    {
        parent::__construct(Socket::TYPE_TCP);
        $this->name = $name;
        $this->port = $port;
        $this->timeout = $timeout ?? $this->getConnectTimeout();

        $this->receiverConstruct(HttpParser::TYPE_RESPONSE, HttpParser::EVENTS_ALL);
    }

    public function sendRawData(string $method = 'GET', string $path = '/', array $headers = [], string $conotents = '', string $version = '1.1')
    {
        try {
            $this->checkLiveness();
        } catch (Socket\Exception $exception) {
            $this->connect($this->name, $this->port, $this->timeout);
        }

        $this->write([packRequest(
            $method,
            $path,
            $headers,
            $conotents,
            $version
        )]);
    }

    public function recvRawData()
    {
        return $this->receiverExecute(
            $this->maxHeaderLength,
            $this->maxContentLength
        );
    }

    public function sendRequest(RequestInterface $request): ResponseInterface
    {
        try {
            $this->sendRawData(
                $request->getMethod(),
                $request->getUri()->getPath(),
                $request->getHeaders(),
                $request->getBody()->getContents(),
                $request->getProtocolVersion()
            );

            [
                $reasonPhrase,
                $statusCode,
                $headers,
                $headerNames,
                $body,
                $contentLength,
                $protocolVersion,
                $shouldKeepAlive,
                $isUpgrade,
            ] = $this->recvRawData();

            return new Response($statusCode, $headers, $body, $reasonPhrase, $protocolVersion);
        } catch (\Exception $exception) {
            $this->throwClientException($exception, $request);
        }
    }

    protected function throwClientException(\Exception $exception, RequestInterface $request): ClientExceptionInterface
    {
        if ($exception instanceof Socket\Exception) {
            throw new NetworkException($request, $exception->getMessage(), $exception->getCode());
        }

        throw new ClientException($exception->getMessage(), $exception->getCode());
    }
}
