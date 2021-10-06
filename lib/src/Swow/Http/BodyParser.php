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

use InvalidArgumentException;
use Psr\Http\Message\ServerRequestInterface;
use function is_array;
use function json_decode;
use function json_last_error;
use function json_last_error_msg;
use function libxml_disable_entity_loader;
use function parse_str;
use function simplexml_load_string;
use function strpos;
use function strtolower;
use function substr;

class BodyParser
{
    public static function parse(ServerRequestInterface $request): array
    {
        $data = [];
        $contentType = static::getContentType($request);
        $contents = $request->getBody()->getContents();
        if ($contents) {
            // TODO: use constants
            switch ($contentType) {
                case 'text/json':
                case 'application/json':
                    $data = static::decodeJson($contents);
                    break;
                case 'text/xml':
                case 'application/xml':
                    $data = static::decodeXml($contents);
                    break;
                default:
                    parse_str($contents, $data);
                    break;
            }
        }

        return $data;
    }

    public static function decodeJson(string $json): array
    {
        $data = json_decode($json, true);
        $code = json_last_error();
        if ($code !== JSON_ERROR_NONE) {
            throw new InvalidArgumentException(json_last_error_msg(), $code);
        }

        return is_array($data) ? $data : [];
    }

    public static function decodeXml(string $xml): array
    {
        $disableLibxmlEntityLoader = libxml_disable_entity_loader(true);
        $respObject = simplexml_load_string($xml, 'SimpleXMLElement', LIBXML_NOCDATA | LIBXML_NOERROR);
        libxml_disable_entity_loader($disableLibxmlEntityLoader);
        if ($respObject === false) {
            throw new InvalidArgumentException('Syntax error.');
        }

        return (array) $respObject;
    }

    public static function getContentType(ServerRequestInterface $request)
    {
        $rawContentType = $request->getHeaderLine('Content-Type');
        if (($pos = strpos($rawContentType, ';')) !== false) {
            // e.g. application/json; charset=UTF-8
            $contentType = strtolower(substr($rawContentType, 0, $pos));
        } else {
            $contentType = strtolower($rawContentType);
        }

        return $contentType;
    }
}
