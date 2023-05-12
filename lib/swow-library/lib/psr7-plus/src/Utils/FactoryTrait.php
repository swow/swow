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

use Psr\Http\Message\RequestFactoryInterface;
use Psr\Http\Message\ResponseFactoryInterface;
use Psr\Http\Message\ServerRequestFactoryInterface;
use Psr\Http\Message\StreamFactoryInterface;
use Psr\Http\Message\UploadedFileFactoryInterface;
use Psr\Http\Message\UriFactoryInterface;
use Swow\Psr7\Message\Psr17Factory;
use Swow\Psr7\Server\ServerConnectionFactoryInterface;

trait FactoryTrait
{
    protected static Psr17Factory $builtinPsr17Factory;
    protected static UriFactoryInterface $uriFactory;
    protected static StreamFactoryInterface $streamFactory;
    protected static RequestFactoryInterface $requestFactory;
    protected static ResponseFactoryInterface $responseFactory;
    protected static ServerRequestFactoryInterface $serverRequestFactory;
    protected static UploadedFileFactoryInterface $uploadedFileFactory;
    protected static ServerConnectionFactoryInterface $serverConnectionFactory;

    public static function getBuiltinPsr17Factory(): Psr17Factory
    {
        return static::$builtinPsr17Factory ??= new Psr17Factory();
    }

    public static function getDefaultUriFactory(): UriFactoryInterface
    {
        return static::$uriFactory ??= static::getBuiltinPsr17Factory();
    }

    public static function setDefaultUriFactory(UriFactoryInterface $uriFactory): void
    {
        static::$uriFactory = $uriFactory;
    }

    public static function getDefaultStreamFactory(): StreamFactoryInterface
    {
        return static::$streamFactory ??= static::getBuiltinPsr17Factory();
    }

    public static function setDefaultStreamFactory(StreamFactoryInterface $streamFactory): void
    {
        static::$streamFactory = $streamFactory;
    }

    public static function getDefaultRequestFactory(): RequestFactoryInterface
    {
        return static::$requestFactory ??= static::getBuiltinPsr17Factory();
    }

    public static function setDefaultRequestFactory(RequestFactoryInterface $requestFactory): void
    {
        static::$requestFactory = $requestFactory;
    }

    public static function getDefaultResponseFactory(): ResponseFactoryInterface
    {
        return static::$responseFactory ??= static::getBuiltinPsr17Factory();
    }

    public static function setDefaultResponseFactory(ResponseFactoryInterface $responseFactory): void
    {
        static::$responseFactory = $responseFactory;
    }

    public static function getDefaultServerRequestFactory(): ServerRequestFactoryInterface
    {
        return static::$serverRequestFactory ??= static::getBuiltinPsr17Factory();
    }

    public static function setDefaultServerRequestFactory(ServerRequestFactoryInterface $serverRequestFactory): void
    {
        static::$serverRequestFactory = $serverRequestFactory;
    }

    public static function getDefaultUploadedFileFactory(): UploadedFileFactoryInterface
    {
        return static::$uploadedFileFactory ??= static::getBuiltinPsr17Factory();
    }

    public static function setDefaultUploadedFileFactory(UploadedFileFactoryInterface $uploadedFileFactory): void
    {
        static::$uploadedFileFactory = $uploadedFileFactory;
    }

    public static function getDefaultServerConnectionFactory(): ServerConnectionFactoryInterface
    {
        return static::$serverConnectionFactory ??= static::getBuiltinPsr17Factory();
    }

    public static function setDefaultServerConnectionFactory(ServerConnectionFactoryInterface $serverConnectionFactory): void
    {
        static::$serverConnectionFactory = $serverConnectionFactory;
    }
}
