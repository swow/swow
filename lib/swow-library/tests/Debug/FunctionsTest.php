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

namespace Swow\Tests\Debug;

use PHPUnit\Framework\Attributes\CoversFunction;
use PHPUnit\Framework\Attributes\DataProvider;
use PHPUnit\Framework\TestCase;
use stdClass;
use Stringable;

use function Swow\Debug\isStrictStringable;
use function Swow\Debug\isStringable;
use function Swow\Debug\var_dump_return;

/** @internal */
#[CoversFunction('Swow\Debug\isStringable')]
#[CoversFunction('Swow\Debug\isStrictStringable')]
#[CoversFunction('Swow\Debug\var_dump_return')]
final class FunctionsTest extends TestCase
{
    #[DataProvider('provideStringableContractsCases')]
    public function testStringableContracts(mixed $value, bool $stringable, bool $strict): void
    {
        $this->assertSame($stringable, isStringable($value));
        $this->assertSame($strict, isStrictStringable($value));
    }

    /** @return iterable<string, array{mixed, bool, bool}> */
    public static function provideStringableContractsCases(): iterable
    {
        yield 'null' => [null, true, false];
        yield 'integer' => [42, true, false];
        yield 'float' => [1.5, true, false];
        yield 'boolean' => [false, true, false];
        yield 'string' => ['value', true, true];
        yield 'array' => [[], false, false];
        yield 'object' => [new stdClass(), false, false];
        yield 'stringable object' => [new class implements Stringable {
            public function __toString(): string
            {
                return 'value';
            }
        }, true, true];
    }

    public function testResourceIsNotStringable(): void
    {
        $resource = fopen('php://temp', 'w+b');
        try {
            $this->assertFalse(isStringable($resource));
            $this->assertFalse(isStrictStringable($resource));
        } finally {
            fclose($resource);
        }
    }

    public function testDumpReturnsOutputWithoutConsumingOuterBuffer(): void
    {
        ob_start();
        try {
            echo 'before';
            $dump = var_dump_return(42, 'abc');
            echo 'after';
            $this->assertSame("int(42)\nstring(3) \"abc\"\n", $dump);
            $this->assertSame('beforeafter', ob_get_contents());
        } finally {
            ob_end_clean();
        }
    }
}
