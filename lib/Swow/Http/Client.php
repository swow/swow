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
use Swow\Http\Client\NetworkException;
use Swow\Http\Client\RequestException;
use Swow\Http\Parser\Exception as ParserException;
use Swow\Socket;
use Swow\Socket\Exception as SocketException;

class Client extends Socket implements ClientInterface
{
    use ConfigTrait;

    use ReceiverTrait {
        __construct as receiverConstruct;
        execute as receiverExecute;
    }

    public function __construct(int $type = Socket::TYPE_TCP)
    {
        parent::__construct($type);
        $this->receiverConstruct(Parser::TYPE_RESPONSE, Parser::EVENTS_ALL);
    }

    public function sendRaw(
        string $method = 'GET',
        string $path = '/',
        array $headers = [],
        string $body = '',
        string $protocolVersion = '1.1'
    ) {
        $this->write([
            packRequest(
                $method,
                $path,
                $headers,
                '',
                $protocolVersion
            ),
            $body,
        ]);
    }

    public function recvRaw(): RawResponse
    {
        return $this->receiverExecute(
            $this->maxHeaderLength,
            $this->maxContentLength
        );
    }

    public function sendRequest(RequestInterface $request): ResponseInterface
    {
        try {
            $this->sendRaw(
                $request->getMethod(),
                $request->getRequestTarget(),
                $request->getHeaders(),
                (string) $request->getBody(),
                $request->getProtocolVersion()
            );

            $result = $this->recvRaw();

            return new Response(
                $result->statusCode,
                $result->headers,
                $result->body,
                $result->reasonPhrase,
                $result->protocolVersion
            );
        } catch (SocketException | ParserException | Exception  $exception) {
            throw $this->convertToClientException($exception, $request);
        }
    }

    protected function convertToClientException(\Exception $exception, RequestInterface $request): ClientExceptionInterface
    {
        if ($exception instanceof SocketException) {
            return new NetworkException($request, $exception->getMessage(), $exception->getCode());
        }

        return new RequestException($request, $exception->getMessage(), $exception->getCode());
    }
}
