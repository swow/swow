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

namespace Swow\Examples\Fastext;

use RuntimeException;
use Swow\Coroutine;
use Swow\Process\ForkProcessManager;
use Swow\Process\ProcessManager;
use Swow\Process\WorkerContext;
use Throwable;

use function dirname;

use const JSON_THROW_ON_ERROR;
use const PHP_OS_FAMILY;

final class Runner
{
    public static function run(string $mode, int $workers, string $runDir, string $action): void
    {
        if ($workers < 1) {
            throw new RuntimeException('The worker count must be positive');
        }

        $error = null;
        if ($mode === 'fork') {
            if (PHP_OS_FAMILY === 'Windows') {
                throw new RuntimeException('fork mode is not supported on Windows');
            }
            $manager = new ForkProcessManager($workers);
            $manager->setMaxRestartCount(0)->setMaxWaitTime(10);
            $start = static function () use ($manager, $runDir, $action): void {
                $manager->start(static function (WorkerContext $context) use ($runDir, $action): void {
                    if (Worker::runFromRunDir($runDir, $action, $context->getId())) {
                        Worker::waitForStop();
                    }
                });
            };
        } elseif ($mode === 'inline') {
            $manager = new ProcessManager($workers);
            $manager->setMaxRestartCount(0)->setMaxWaitTime(10);
            $code = 'require ' . var_export(dirname(__DIR__) . '/autoload.php', true) . ';'
                . '$ok = \Swow\Examples\Fastext\Worker::runFromEnvironment('
                . var_export($runDir, true) . ', ' . var_export($action, true) . ');'
                . 'if (!$ok) { exit(1); }'
                . '\Swow\Examples\Fastext\Worker::waitForStop();';
            $start = static function () use ($manager, $code): void {
                $manager->startInline($code);
            };
        } elseif ($mode === 'script') {
            $manager = new ProcessManager($workers);
            $manager->setMaxRestartCount(0)->setMaxWaitTime(10);
            $start = static function () use ($manager, $runDir, $action): void {
                $manager->startScript(dirname(__DIR__) . '/worker.php', [$action, $runDir]);
            };
        } else {
            throw new RuntimeException("Unknown manager mode: {$mode}");
        }

        $coroutine = Coroutine::run(static function () use ($start, &$error): void {
            try {
                $start();
            } catch (Throwable $e) {
                $error = $e;
            }
        });

        $deadline = microtime(true) + 3600;
        try {
            while (microtime(true) < $deadline) {
                if ($error !== null) {
                    break;
                }
                $done = 0;
                for ($id = 0; $id < $workers; $id++) {
                    if (is_file($runDir . '/' . $action . '-' . $id . '.result.json')) {
                        $done++;
                    }
                }
                if ($done === $workers) {
                    break;
                }
                usleep(100000);
            }
        } finally {
            try {
                $manager->stop();
            } catch (Throwable) {
            }
        }

        if ($error !== null) {
            throw $error;
        }

        for ($id = 0; $id < $workers; $id++) {
            $path = $runDir . '/' . $action . '-' . $id . '.result.json';
            if (!is_file($path)) {
                throw new RuntimeException("Worker {$id} did not finish");
            }
            $result = json_decode((string) file_get_contents($path), true, 512, JSON_THROW_ON_ERROR);
            if (!($result['ok'] ?? false)) {
                throw new RuntimeException("Worker {$id} failed: " . ($result['error'] ?? 'unknown error'));
            }
        }

        if ($coroutine->isAvailable()) {
            usleep(10000);
        }
    }
}
