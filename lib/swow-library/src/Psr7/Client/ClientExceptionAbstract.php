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

namespace Swow\Psr7\Client;

use Exception;
use Psr\Http\Message\RequestInterface;
use Swow\Exception\ExceptionFakerTrait;
use Throwable;

abstract class ClientExceptionAbstract extends \Swow\Exception implements ClientExceptionInterface
{
    use ExceptionFakerTrait;

    public function __construct(protected RequestInterface $request, string $message = '', int $code = 0, ?Throwable $previous = null)
    {
        parent::__construct($message, $code, $previous);
    }

    public function getRequest(): RequestInterface
    {
        return $this->request;
    }

    public static function fromException(Exception $exception, RequestInterface $request): static
    {
        return (new static(
            $request,
            $exception->getMessage(),
            $exception->getCode(),
            $exception->getPrevious()
        ))->cloneInternalPropertiesFrom($exception);
    }
}
