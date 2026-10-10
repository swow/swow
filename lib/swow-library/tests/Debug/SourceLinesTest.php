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
use Swow\Coroutine;
use Swow\Utils\Handler;
use TypeError;
use WeakReference;

use function Swow\Debug\registerDeadlockDetectorHandler;
use function Swow\Debug\showExecutedSourceLines;

/** @internal */
#[CoversFunction('Swow\Debug\showExecutedSourceLines')]
#[CoversFunction('Swow\Debug\registerDeadlockDetectorHandler')]
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

    #[DataProvider('provideFileFiltersSelectRealExecutedSourceCases')]
    public function testFileFiltersSelectRealExecutedSource(string $mode): void
    {
        $filename = tempnam(sys_get_temp_dir(), 'swow-source-lines-');
        file_put_contents($filename, <<<'PHP'
<?php
$sum = 0;
for ($index = 0; $index < 3; $index++) {
    $sum += $index; // source-line-marker
    Swow\Coroutine::getCurrent();
}
return $sum;
PHP);
        $filter = match ($mode) {
            'filename' => $filename,
            'slash regex' => '/' . preg_quote($filename, '/') . '$/',
            'hash regex' => '#' . preg_quote($filename, '#') . '$#',
            'file list' => [$filename],
            'callable' => static fn(?string $file, ?int $line): bool => $file === $filename,
            'all files' => true,
        };
        ob_start();
        try {
            showExecutedSourceLines($filter, force: true);
            $result = require $filename;
            showExecutedSourceLines(null);
        } finally {
            showExecutedSourceLines(null);
            $output = ob_get_clean();
            unlink($filename);
        }
        $this->assertSame(3, $result);
        $this->assertStringContainsString('>>> ' . $filename, $output);
        $this->assertStringContainsString('source-line-marker', $output);
    }

    /** @return iterable<string, array{string}> */
    public static function provideFileFiltersSelectRealExecutedSourceCases(): iterable
    {
        foreach (['filename', 'slash regex', 'hash regex', 'file list', 'callable', 'all files'] as $mode) {
            yield $mode => [$mode];
        }
    }

    public function testDefaultFilterStaysInTheCallingFileAndCanBeReplaced(): void
    {
        ob_start();
        try {
            showExecutedSourceLines(false, force: true);
            for ($index = 0; $index < 2; $index++) { // default-source-marker
                Coroutine::getCurrent();
            }
            showExecutedSourceLines(static fn(?string $file, ?int $line): bool => false, force: true);
            echo 'after-replacement';
            showExecutedSourceLines(null);
        } finally {
            showExecutedSourceLines(null);
            $output = ob_get_clean();
        }
        $this->assertStringContainsString('default-source-marker', $output);
        $this->assertStringEndsWith('after-replacement', $output);
    }

    public function testDeadlockDetectorRegistrationCanBeRemoved(): void
    {
        $handler = registerDeadlockDetectorHandler();
        $this->assertInstanceOf(Handler::class, $handler);
        $reference = WeakReference::create($handler);
        $handler->remove();
        $handler->remove();
        unset($handler);
        gc_collect_cycles();
        $this->assertNull($reference->get());
    }

    public function testInvalidFilterReportsItsType(): void
    {
        $this->expectException(TypeError::class);
        $this->expectExceptionMessage('int given');
        showExecutedSourceLines(42);
    }
}
