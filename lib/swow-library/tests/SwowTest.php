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

namespace Swow\Tests;

use PHPUnit\Framework\TestCase;
use Swow;

use function extension_loaded;
use function strlen;

/**
 * @internal
 * @coversNothing
 */
final class SwowTest extends TestCase
{
    public function testSwowExtension(): void
    {
        $this->assertTrue(extension_loaded(Swow::class));
        $this->assertGreaterThan(0, Swow\Extension::VERSION_ID);
    }

    public function testSwowLibrary(): void
    {
        $this->assertTrue(Swow::isLibraryLoaded());
        $this->assertSame(Swow\Library::VERSION, sprintf(
            '%d.%d.%d%s%s',
            Swow\Library::MAJOR_VERSION,
            Swow\Library::MINOR_VERSION,
            Swow\Library::RELEASE_VERSION,
            strlen(Swow\Library::EXTRA_VERSION) > 0 ? '-' : '', Swow\Library::EXTRA_VERSION
        ));
        $this->assertSame(Swow\Library::VERSION_ID, (int) sprintf(
            '%02d%02d%02d',
            Swow\Library::MAJOR_VERSION,
            Swow\Library::MINOR_VERSION,
            Swow\Library::RELEASE_VERSION
        ));
    }
}
