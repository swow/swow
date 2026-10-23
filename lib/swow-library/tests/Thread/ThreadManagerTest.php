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

namespace Swow\Tests\Thread;

use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\TestCase;
use Swow\Coroutine;
use Swow\ThreadManager;
use Swow\ThreadManagerException;
use Swow\ThreadManagerInterface;
use Swow\ThreadWorkerContext;

use function filter_var;
use function ini_get;
use function sprintf;

use const FILTER_VALIDATE_BOOLEAN;
use const JSON_THROW_ON_ERROR;

/**
 * @internal
 */
#[CoversClass(ThreadManager::class)]
final class ThreadManagerTest extends TestCase
{
    public function testInterface(): void
    {
        $this->assertInstanceOf(ThreadManagerInterface::class, new ThreadManager(1));
    }

    public function testWorkerCount(): void
    {
        $this->assertSame(4, (new ThreadManager(4))->getWorkerCount());
        $this->assertGreaterThan(0, (new ThreadManager(0))->getWorkerCount());
        $this->assertSame(\Swow\nproc(), (new ThreadManager(0))->getWorkerCount());
    }

    public function testFluentSetters(): void
    {
        $manager = new ThreadManager(1);
        $this->assertSame($manager, $manager->setMaxRestartCount(3));
        $this->assertSame($manager, $manager->setMaxWaitTime(2));
        $this->assertSame($manager, $manager->onWorkerExit(static function (): void {
        }));
    }

    public function testStartInlineWorker(): void
    {
        $file = $this->makeTempFile();
        $code = 'file_put_contents(' . var_export($file, true) . ', (string) $context->getId());';

        $manager = new ThreadManager(1);
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

    public function testInlineContext(): void
    {
        $file = $this->makeTempFile();
        $code = <<<'PHP'
        file_put_contents(
            %s,
            json_encode([
                'id' => $context->getId(),
                'threadId' => $context->getThreadId(),
                'managerThreadId' => $context->getManagerThreadId(),
                'workerCount' => $context->getWorkerCount(),
            ])
        );
        PHP;
        $code = sprintf($code, var_export($file, true));

        $manager = new ThreadManager(1);
        $manager->setMaxRestartCount(0)->setMaxWaitTime(1);
        $this->runManager(static function () use ($manager, $code): void {
            $manager->startInline($code);
        });

        try {
            $data = json_decode($this->waitForFile($file), true, 512, JSON_THROW_ON_ERROR);
            $this->assertSame(0, $data['id']);
            $this->assertGreaterThan(0, $data['threadId']);
            $this->assertGreaterThan(0, $data['managerThreadId']);
            $this->assertSame(1, $data['workerCount']);
        } finally {
            $manager->stop();
            @unlink($file);
        }
    }

    public function testStartInlineEmptyCode(): void
    {
        $manager = new ThreadManager(1);
        $this->expectException(ThreadManagerException::class);
        $manager->startInline('');
    }

    public function testStartClosureRequiresSerializer(): void
    {
        if ($this->isClosureSerializerEnabled()) {
            $this->markTestSkipped('swow.closure_serializer is enabled');
        }

        $manager = new ThreadManager(1);
        $this->expectException(ThreadManagerException::class);
        $manager->start(static function (ThreadWorkerContext $context): void {
        });
    }

    public function testStartClosureWorker(): void
    {
        if (!$this->isClosureSerializerEnabled()) {
            $this->markTestSkipped('swow.closure_serializer is disabled');
        }

        $file = $this->makeTempFile();
        $closure = static function (ThreadWorkerContext $context) use ($file): void {
            file_put_contents($file, (string) $context->getId());
        };

        $manager = new ThreadManager(1);
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

    public function testStopInlineWorker(): void
    {
        $manager = new ThreadManager(1);
        $manager->setMaxWaitTime(1);
        $this->runManager(static function () use ($manager): void {
            $manager->startInline('while (true) { usleep(10000); }');
        });

        usleep(100000);
        $started = microtime(true);
        $manager->stop();

        $this->assertLessThan(2.0, microtime(true) - $started);
    }

    public function testReload(): void
    {
        $file = $this->makeTempFile();
        $code = 'file_put_contents(' . var_export($file, true) . ', (string) $context->getThreadId());'
            . ' usleep(500000);';

        $manager = new ThreadManager(1);
        $manager->setMaxRestartCount(0)->setMaxWaitTime(1);
        $this->runManager(static function () use ($manager, $code): void {
            $manager->startInline($code);
        });

        try {
            $oldThreadId = $this->waitForFile($file);
            $manager->reload();
            $this->waitFor(static function () use ($file, $oldThreadId): bool {
                return is_file($file) && file_get_contents($file) !== $oldThreadId;
            });
            $this->assertNotSame($oldThreadId, file_get_contents($file));
        } finally {
            $manager->stop();
            @unlink($file);
        }
    }

    public function testWorkerRestart(): void
    {
        $file = $this->makeTempFile();
        $code = 'file_put_contents(' . var_export($file, true) . ', "x", FILE_APPEND);';

        $manager = new ThreadManager(1);
        $manager->setMaxRestartCount(1)->setMaxWaitTime(1);
        $this->runManager(static function () use ($manager, $code): void {
            $manager->startInline($code);
        });

        try {
            $this->waitFor(static function () use ($file): bool {
                return is_file($file) && filesize($file) >= 2;
            });
            $this->assertGreaterThanOrEqual(2, filesize($file));
        } finally {
            $manager->stop();
            @unlink($file);
        }
    }

    public function testWorkerExitCallback(): void
    {
        $status = null;

        $manager = new ThreadManager(1);
        $manager->setMaxRestartCount(0)->setMaxWaitTime(1);
        $manager->onWorkerExit(static function (int $workerId, int $threadId, int $exitStatus) use (&$status): void {
            $status = [$workerId, $threadId, $exitStatus];
        });
        $this->runManager(static function () use ($manager): void {
            $manager->startInline('throw new RuntimeException("worker failure");');
        });

        try {
            $this->waitFor(static function () use (&$status): bool {
                return $status !== null;
            });
            $this->assertSame(0, $status[0]);
            $this->assertGreaterThan(0, $status[1]);
            $this->assertSame(255, $status[2]);
        } finally {
            $manager->stop();
        }
    }

    private function runManager(callable $start): void
    {
        $coroutine = Coroutine::run($start);
        $this->assertTrue($coroutine->isAvailable());
    }

    private function isClosureSerializerEnabled(): bool
    {
        return filter_var(ini_get('swow.closure_serializer'), FILTER_VALIDATE_BOOLEAN);
    }

    private function makeTempFile(): string
    {
        $file = tempnam(sys_get_temp_dir(), 'swow-tm-');
        $this->assertIsString($file);
        @unlink($file);

        return $file;
    }

    private function waitFor(callable $condition): void
    {
        $deadline = microtime(true) + 5.0;
        do {
            clearstatcache();
            if ($condition()) {
                return;
            }
            usleep(10000);
        } while (microtime(true) < $deadline);

        $this->fail('Condition was not met');
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
