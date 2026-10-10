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

namespace Swow\Tests\Exception;

use Exception;
use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\TestCase;
use Swow\Exception\ExceptionEditor;
use Swow\Psr7\Client\ClientExceptionAbstract;
use Swow\Psr7\Client\ClientRequestException;
use Swow\Psr7\Psr7;

/** @internal */
#[CoversClass(ExceptionEditor::class)]
#[CoversClass(ClientExceptionAbstract::class)]
#[CoversClass(ClientRequestException::class)]
final class ExceptionEditorTest extends TestCase
{
    public function testEditingPropertiesPreservesExceptionIdentityAndCause(): void
    {
        $cause = new Exception('cause');
        $exception = new Exception('original', 1, $cause);
        $trace = [['file' => '/project/caller.php', 'line' => 5, 'function' => 'receive', 'args' => ['data']]];
        ExceptionEditor::setCode($exception, 42);
        ExceptionEditor::setMessage($exception, 'edited');
        ExceptionEditor::setFile($exception, '/project/source.php');
        ExceptionEditor::setLine($exception, 12);
        ExceptionEditor::setTrace($exception, $trace);
        $this->assertSame(42, $exception->getCode());
        $this->assertSame('edited', $exception->getMessage());
        $this->assertSame('/project/source.php', $exception->getFile());
        $this->assertSame(12, $exception->getLine());
        $this->assertSame($trace, $exception->getTrace());
        $this->assertSame($cause, $exception->getPrevious());
    }

    public function testClientWrapperRetainsOriginalSourceAndRequest(): void
    {
        $cause = new Exception('cause');
        $exception = new Exception('failed', 42, $cause);
        $request = Psr7::createRequest('POST', 'http://example.test/');
        $wrapped = ClientRequestException::fromException($exception, $request);
        $this->assertSame($request, $wrapped->getRequest());
        $this->assertSame($exception->getMessage(), $wrapped->getMessage());
        $this->assertSame($exception->getCode(), $wrapped->getCode());
        $this->assertSame($exception->getFile(), $wrapped->getFile());
        $this->assertSame($exception->getLine(), $wrapped->getLine());
        $this->assertSame($exception->getTrace(), $wrapped->getTrace());
        $this->assertSame($cause, $wrapped->getPrevious());
        $this->assertNotSame($exception, $wrapped);
    }
}
