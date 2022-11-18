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

namespace Swow\Tests\Binary;

use PHPUnit\Framework\TestCase;

use function array_map;
use function dirname;
use function file_exists;
use function file_put_contents;
use function implode;
use function realpath;
use function shell_exec;
use function sprintf;
use function unlink;

use const PHP_BINARY;

/**
 * @coversNothing
 * @internal
 */
final class BuilderTest extends TestCase
{
    protected static string $builderPath;

    protected function setUp(): void
    {
        parent::setUp();
        if (!file_exists(self::$builderPath ??= dirname(__DIR__, 4) . '/ext/bin/swow-builder')) {
            $this->markTestSkipped('Unable to detect builder binary file');
        }
    }

    /**
     * @param array<string> $longOptions
     */
    protected function exec(array $longOptions): string
    {
        return shell_exec(sprintf(
            '%s %s --dry-run -q --rebuild %s',
            realpath(PHP_BINARY),
            self::$builderPath,
            implode(' ', array_map(static fn(string $o) => "--{$o}", $longOptions))
        ));
    }

    public function testConfiguration(): void
    {
        $output = $this->exec(longOptions: ['sudo']);
        $this->assertStringContainsString('sudo make install', $output);
        $output = $this->exec(longOptions: ['clean']);
        $this->assertStringContainsString('phpize --clean', $output);
        $makefilePath = dirname(__DIR__, 4) . '/ext/Makefile';
        if (!($makefileExists = file_exists($makefilePath))) {
            file_put_contents($makefilePath, '');
        }
        $this->assertStringContainsString('cleaner.sh', $output);
        if (!$makefileExists) {
            unlink($makefilePath);
        }
        $output = $this->exec(longOptions: ['debug']);
        $this->assertStringContainsString('--enable-swow-debug', $output);
        $output = $this->exec(longOptions: ['msan']);
        $this->assertStringContainsString('--enable-swow-memory-sanitizer', $output);
        $output = $this->exec(longOptions: ['asan']);
        $this->assertStringContainsString('--enable-swow-address-sanitizer', $output);
        $output = $this->exec(longOptions: ['ubsan']);
        $this->assertStringContainsString('--enable-swow-undefined-sanitizer', $output);
        $output = $this->exec(longOptions: ['valgrind']);
        $this->assertStringContainsString('--enable-swow-valgrind', $output);
        $output = $this->exec(longOptions: ['gcov']);
        $this->assertStringContainsString('--enable-swow-gcov', $output);
        $output = $this->exec(longOptions: ['thread-context']);
        $this->assertStringContainsString('--enable-swow-thread-context', $output);
        $output = $this->exec(longOptions: ['ssl']);
        $this->assertStringContainsString('--enable-swow-ssl', $output);
        $this->assertStringNotContainsString('--enable-swow-curl', $output);
        $output = $this->exec(longOptions: ['curl']);
        $this->assertStringContainsString('--enable-swow-curl', $output);
        $this->assertStringNotContainsString('--enable-swow-ssl', $output);
        $output = $this->exec(longOptions: ['ssl', 'curl']);
        $this->assertStringContainsString('--enable-swow-ssl', $output);
        $this->assertStringContainsString('--enable-swow-curl', $output);
        $output = $this->exec(longOptions: ['php-config']);
        $this->assertStringContainsString('--with-php-config', $output);
        $output = $this->exec(longOptions: ['php-config=/path/to/php-config']);
        $this->assertStringContainsString('--with-php-config=/path/to/php-config', $output);
    }
}
