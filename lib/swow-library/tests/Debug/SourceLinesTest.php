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
use PHPUnit\Framework\TestCase;

use function Swow\Debug\showExecutedSourceLines;

/** @internal */
#[CoversFunction('Swow\Debug\showExecutedSourceLines')]
final class SourceLinesTest extends TestCase
{
    public function testStoppingBeforeRegistrationIsHarmless(): void
    {
        ob_start();
        try {
            showExecutedSourceLines(null);
            showExecutedSourceLines(null);
        } finally {
            $output = ob_get_clean();
        }
        $this->assertSame('', $output);
    }
}
