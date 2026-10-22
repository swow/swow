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

namespace Swow\Tests\Process;

use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\TestCase;
use Swow\Coroutine;
use Swow\Process\ProcessManager;
use Swow\Process\ProcessManagerInterface;
use Swow\Process\WorkerContext;

use function ini_get;

use const FILTER_VALIDATE_BOOLEAN;

/**
 * @internal
 */
#[CoversClass(ProcessManager::class)]
final class ProcessManagerTest extends TestCase
{
    public function testInterface(): void
    {
        $this->assertInstanceOf(ProcessManagerInterface::class, new ProcessManager(1));
    }

    public function testStartInlineWorker(): void
    {
        $file = $this->makeTempFile();
        $code = 'file_put_contents(' . var_export($file, true) . ', getenv("SWOW_PROCESS_MANAGER_WORKER_ID"));';

        $manager = new ProcessManager(1);
        $manager->setMaxRestartCount(0)->setMaxWaitTime(1);
        $this->runManager(static function () use ($manager, $code): void {
            $manager->startInline($code);
        });

        try {
            $this->assertSame('0', $this->waitForFile($file));
        } finally {
            $manager->stop();
            @unlink($file);
        }
    }

    public function testStartScriptWorker(): void
    {
        $file = $this->makeTempFile();
        $script = $this->makeTempFile();
        file_put_contents($script, '<?php file_put_contents(' . var_export($file, true) . ', getenv("SWOW_PROCESS_MANAGER_WORKER_ID"));');

        $manager = new ProcessManager(1);
        $manager->setMaxRestartCount(0)->setMaxWaitTime(1);
        $this->runManager(static function () use ($manager, $script): void {
            $manager->startScript($script);
        });

        try {
            $this->assertSame('0', $this->waitForFile($file));
        } finally {
            $manager->stop();
            @unlink($file);
            @unlink($script);
        }
    }

    public function testStartClosureWorker(): void
    {
        if (!filter_var(ini_get('swow.closure_serializer'), FILTER_VALIDATE_BOOLEAN)) {
            $this->markTestSkipped('swow.closure_serializer is not enabled');
        }

        $file = $this->makeTempFile();
        $closure = static function (WorkerContext $context) use ($file): void {
            file_put_contents($file, (string) $context->getId());
        };

        $manager = new ProcessManager(1);
        $manager->setMaxRestartCount(0)->setMaxWaitTime(1);
        $this->runManager(static function () use ($manager, $closure): void {
            $manager->start($closure);
        });

        try {
            $this->assertSame('0', $this->waitForFile($file));
        } finally {
            $manager->stop();
            @unlink($file);
        }
    }

    public function testStartInlineEmptyCode(): void
    {
        $manager = new ProcessManager(1);
        $this->expectException(\Swow\Process\ProcessManagerException::class);
        $manager->startInline('');
    }

    public function testStartScriptMissingFile(): void
    {
        $manager = new ProcessManager(1);
        $this->expectException(\Swow\Process\ProcessManagerException::class);
        $manager->startScript(__DIR__ . '/missing-worker-script.php');
    }

    private function runManager(callable $start): void
    {
        $coroutine = Coroutine::run($start);
        $this->assertTrue($coroutine->isAvailable());
    }

    private function makeTempFile(): string
    {
        $file = tempnam(sys_get_temp_dir(), 'swow-pm-');
        $this->assertIsString($file);
        @unlink($file);

        return $file;
    }

    private function waitForFile(string $file): string
    {
        $deadline = microtime(true) + 5.0;
        do {
            clearstatcache(true, $file);
            if (is_file($file)) {
                $content = file_get_contents($file);
                $this->assertIsString($content);
                if ($content !== '') {
                    return $content;
                }
            }
            usleep(10000);
        } while (microtime(true) < $deadline);

        $this->fail("Worker did not write {$file}");
    }
}
