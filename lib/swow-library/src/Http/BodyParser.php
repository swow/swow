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

use InvalidArgumentException;
use Psr\Http\Message\StreamInterface;
use SimpleXMLElement;
use function is_array;
use function json_decode;
use function json_last_error;
use function json_last_error_msg;
use function parse_str;
use function simplexml_load_string;
use const JSON_ERROR_NONE;
use const LIBXML_NOCDATA;
use const LIBXML_NOERROR;

class BodyParser
{
    /** @return array<mixed> */
    public static function parse(StreamInterface $stream, string $contentType = MimeType::X_WWW_FORM_URLENCODED): array
    {
        $contents = (string) $stream;
        if (!$contents) {
            return [];
        }
        switch ($contentType) {
            case MimeType::JSON:
                $data = static::decodeJson($contents);
                break;
            case MimeType::XML:
                $data = static::decodeXml($contents);
                break;
            default:
                parse_str($contents, $data);
                break;
        }

        return $data;
    }

    /** @return array<mixed> */
    protected static function decodeJson(string $json): array
    {
        $data = json_decode($json, true);
        $code = json_last_error();
        if ($code !== JSON_ERROR_NONE) {
            throw new InvalidArgumentException(json_last_error_msg(), $code);
        }

        return is_array($data) ? $data : [];
    }

    /** @return array<mixed> */
    protected static function decodeXml(string $xml): array
    {
        $respObject = simplexml_load_string($xml, SimpleXMLElement::class, LIBXML_NOCDATA | LIBXML_NOERROR);
        if ($respObject === false) {
            throw new InvalidArgumentException('Syntax error.');
        }

        return (array) $respObject;
    }
}
