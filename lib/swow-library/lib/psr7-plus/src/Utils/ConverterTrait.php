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

namespace Swow\Psr7\Utils;

use Psr\Http\Message\RequestInterface;
use Psr\Http\Message\ResponseInterface;
use Swow\Http\Http;
use Swow\Psr7\Message\RequestPlusInterface;
use Swow\Psr7\Message\ResponsePlusInterface;

trait ConverterTrait
{
    /**
     * @return array<string>
     * @phan-return array{0: string, 1: string}
     * @phpstan-return array{0: string, 1: string}
     * @psalm-return array{0: string, 1: string}
     */
    public static function convertResponseToVector(ResponseInterface $response): array
    {
        if ($response instanceof ResponsePlusInterface) {
            return [
                $response->toString(true),
                (string) $response->getBody(),
            ];
        }
        return [
            Http::packResponse(
                statusCode: $response->getStatusCode(),
                reasonPhrase: $response->getReasonPhrase(),
                headers: $response->getHeaders(),
                protocolVersion: $response->getProtocolVersion()
            ),
            (string) $response->getBody(),
        ];
    }

    public static function stringifyRequest(RequestInterface $request, bool $withoutBody = false): string
    {
        if ($request instanceof RequestPlusInterface) {
            return $request->toString($withoutBody);
        }
        return Http::packRequest(
            method: $request->getMethod(),
            uri: (string) $request->getUri(),
            headers: $request->getHeaders(),
            body: $withoutBody ? '' : (string) $request->getBody(),
            protocolVersion: $request->getProtocolVersion()
        );
    }

    public static function stringifyResponse(ResponseInterface $response, bool $withoutBody = false): string
    {
        if ($response instanceof ResponsePlusInterface) {
            return $response->toString($withoutBody);
        }
        return Http::packResponse(
            statusCode: $response->getStatusCode(),
            reasonPhrase: $response->getReasonPhrase(),
            headers: $response->getHeaders(),
            body: $withoutBody ? '' : (string) $response->getBody(),
            protocolVersion: $response->getProtocolVersion()
        );
    }
}
