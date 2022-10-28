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

namespace Swow\Http\Protocol;

use Swow\Exception;
use Swow\Http\Status;
use Throwable;

/**
 * After this exception was thrown, Server should send response to client
 * with this statusCode and reasonPhrase, and Connection should be closed.
 */
class ProtocolException extends Exception
{
    public function __construct(int $statusCode, string $reasonPhrase = '', ?Throwable $previous = null)
    {
        parent::__construct($reasonPhrase ?: Status::getReasonPhraseOf($statusCode), $statusCode, $previous);
    }
}
