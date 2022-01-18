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

namespace SwowTest\Stream;

use PHPUnit\Framework\TestCase;
use Swow\Stream\Buffer;

/**
 * @internal
 * @coversNothing
 */
final class BufferTest extends TestCase
{
    public function testCreateBuffer(): void
    {
        $buffer = Buffer::for('Hello Swow');

        $this->assertSame('Hello Swow', $buffer->read());
    }
}
