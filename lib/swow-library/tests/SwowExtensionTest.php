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

namespace SwowTest;

use PHPUnit\Framework\TestCase;
use function defined;
use function extension_loaded;

/**
 * @internal
 * @coversNothing
 */
final class SwowExtensionTest extends TestCase
{
    public function testSwowExtension(): void
    {
        $this->assertTrue(extension_loaded('swow'));
    }

    public function testSwowLibrary(): void
    {
        $this->assertTrue(defined('SWOW_LIBRARY'));
        $this->assertTrue(SWOW_LIBRARY);
    }
}
