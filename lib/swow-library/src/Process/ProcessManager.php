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

namespace Swow\Process;

use Closure;
use ReflectionClass;
use Throwable;

use function array_merge;
use function count;
use function dirname;
use function extension_loaded;
use function fclose;
use function fwrite;
use function getenv;
use function getmypid;
use function ini_get;
use function is_array;
use function is_file;
use function is_resource;
use function is_string;
use function str_contains;
use function strlen;

use const DIRECTORY_SEPARATOR;
use const FILTER_VALIDATE_BOOLEAN;
use const PHP_BINARY;
use const PHP_OS_FAMILY;

/**
 * Cross-platform process manager backed by ChildProcess::spawn().
 */
final class ProcessManager implements ProcessManagerInterface
{
    use ProcessManagerTrait;

    private const SIGNAL_POLL_TIMEOUT = 100;

    private string $mode = '';

    private ?string $serializedClosure = null;

    private ?string $inlineCode = null;

    private ?string $scriptPath = null;

    /** @var list<string> */
    private array $scriptArgs = [];

    /** @var list<string>|null */
    private ?array $phpArgs = null;

    private ?string $autoloadPath = null;

    public function start(Closure $onWorkerStart): void
    {
        $this->assertClosureSerializerEnabled();
        $this->phpArgs = $this->detectPhpArgs(true);
        $this->assertChildClosureSerializer($this->phpArgs);
        $this->serializedClosure = $this->serializeClosure($onWorkerStart);
        $this->autoloadPath = $this->findAutoloadPath();
        $this->mode = 'closure';

        $this->startManager();
    }

    public function startInline(string $code): void
    {
        if ($code === '') {
            throw new ProcessManagerException('Inline worker code must not be empty');
        }

        $this->inlineCode = $code;
        $this->phpArgs = $this->detectPhpArgs(false);
        $this->mode = 'inline';

        $this->startManager();
    }

    /** @param list<string> $args */
    public function startScript(string $script, array $args = []): void
    {
        if (!is_file($script)) {
            throw new ProcessManagerException("Worker script '{$script}' does not exist");
        }
        foreach ($args as $arg) {
            if (!is_string($arg)) {
                throw new ProcessManagerException('Worker script arguments must be strings');
            }
        }

        $this->scriptPath = $script;
        $this->scriptArgs = array_values($args);
        $this->phpArgs = $this->detectPhpArgs(false);
        $this->mode = 'script';

        $this->startManager();
    }

    protected function createWorker(int $id): ChildProcess
    {
        $env = $this->buildEnv($id);
        $options = ['env' => $env];

        switch ($this->mode) {
            case 'closure':
                {
                    $pipe = \Swow\pipe();
                    if (!is_array($pipe) || count($pipe) !== 2) {
                        throw new ProcessManagerException('Unable to create worker payload pipe');
                    }
                    [$read, $write] = $pipe;
                    $env['SWOW_PROCESS_MANAGER_PAYLOAD_FD'] = '3';
                    $options['env'] = $env;
                    $options['fds'] = [3 => $read];

                    $command = array_merge(
                        [PHP_BINARY],
                        $this->phpArgs ?? [],
                        [__DIR__ . '/process-manager-worker.php'],
                    );

                    try {
                        $process = ChildProcess::spawn($command, $options);
                    } catch (Throwable $e) {
                        fclose($read);
                        fclose($write);
                        throw $e;
                    }

                    fclose($read);

                    $payload = $this->serializedClosure ?? '';
                    $written = 0;
                    try {
                        while ($written < strlen($payload)) {
                            $n = fwrite($write, substr($payload, $written));
                            if ($n === false || $n === 0) {
                                break;
                            }
                            $written += $n;
                        }
                    } finally {
                        fclose($write);
                    }

                    if ($written !== strlen($payload)) {
                        $process->kill(\Swow\Signal::KILL);
                        throw new ProcessManagerException('Unable to write worker payload');
                    }

                    $this->workers[$id] = $process;
                    $this->restartCounts[$id] ??= 0;
                    return $process;
                }
            case 'inline':
                {
                    $command = array_merge(
                        [PHP_BINARY],
                        $this->phpArgs ?? [],
                        ['-r', $this->inlineCode ?? ''],
                    );
                    break;
                }
            case 'script':
                {
                    $command = array_merge(
                        [PHP_BINARY],
                        $this->phpArgs ?? [],
                        [$this->scriptPath ?? ''],
                        $this->scriptArgs,
                    );
                    break;
                }
            default:
                throw new ProcessManagerException('ProcessManager has not been started');
        }

        $process = ChildProcess::spawn($command, $options);
        $this->workers[$id] = $process;
        $this->restartCounts[$id] ??= 0;

        return $process;
    }

    /** @return array<string, string> */
    private function buildEnv(int $id): array
    {
        $env = getenv();
        if (!is_array($env)) {
            $env = [];
        }

        $env['SWOW_PROCESS_MANAGER_WORKER'] = '1';
        $env['SWOW_PROCESS_MANAGER_WORKER_ID'] = (string) $id;
        $env['SWOW_PROCESS_MANAGER_MANAGER_PID'] = (string) getmypid();
        $env['SWOW_PROCESS_MANAGER_WORKER_COUNT'] = (string) $this->workerCount;

        if ($this->autoloadPath !== null) {
            $env['SWOW_PROCESS_MANAGER_AUTOLOAD'] = $this->autoloadPath;
        }

        return $env;
    }

    private function assertClosureSerializerEnabled(): void
    {
        if (!extension_loaded('swow')) {
            throw new ProcessManagerException('The swow extension is required for closure workers');
        }
        if (!filter_var(ini_get('swow.closure_serializer'), FILTER_VALIDATE_BOOLEAN)) {
            throw new ProcessManagerException('swow.closure_serializer is not enabled');
        }
    }

    private function serializeClosure(Closure $closure): string
    {
        try {
            $payload = serialize($closure);
        } catch (Throwable $e) {
            throw new ProcessManagerException('Unable to serialize worker closure', 0, $e);
        }
        if (!is_string($payload) || $payload === '') {
            throw new ProcessManagerException('Unable to serialize worker closure');
        }

        return $payload;
    }

    private function assertChildClosureSerializer(array $phpArgs): void
    {
        [$output] = $this->runPhp($phpArgs, ['-r', 'echo ini_get("swow.closure_serializer");']);
        if (!filter_var(trim($output), FILTER_VALIDATE_BOOLEAN)) {
            throw new ProcessManagerException('swow.closure_serializer is not enabled in the worker PHP configuration');
        }
    }

    /** @return list<string> */
    private function detectPhpArgs(bool $requireSwow): array
    {
        static $cache = [];

        $key = PHP_BINARY . ($requireSwow ? '|1' : '|0');
        if (isset($cache[$key])) {
            return $cache[$key];
        }

        $candidates = [
            [],
            ['-d', 'extension=swow'],
        ];
        $extensionFile = getenv('SWOW_PROCESS_MANAGER_EXTENSION');
        if (!is_string($extensionFile) || $extensionFile === '') {
            $extensionDir = ini_get('extension_dir');
            if (is_string($extensionDir) && $extensionDir !== '') {
                $extensionFile = $extensionDir . DIRECTORY_SEPARATOR . 'swow.' . (PHP_OS_FAMILY === 'Windows' ? 'dll' : 'so');
            }
        }
        if (is_string($extensionFile) && $extensionFile !== '' && is_file($extensionFile)) {
            $candidates[] = ['-d', 'extension=' . $extensionFile];
        }

        foreach ($candidates as $args) {
            [$output] = $this->runPhp($args, ['--ri', 'swow']);
            if (str_contains($output, 'swow') && !str_contains($output, 'not present')) {
                return $cache[$key] = $args;
            }
        }

        if ($requireSwow) {
            throw new ProcessManagerException('Unable to load swow in the worker PHP process');
        }

        return $cache[$key] = [];
    }

    /** @param list<string> $args @param list<string> $scriptArgs @return array{0: string, 1: int} */
    private function runPhp(array $args, array $scriptArgs): array
    {
        $command = array_merge([PHP_BINARY], $args, $scriptArgs);
        $pipes = [];
        $process = proc_open($command, [
            1 => ['pipe', 'w'],
            2 => ['pipe', 'w'],
        ], $pipes);
        if (!is_resource($process)) {
            return ['', -1];
        }

        $stdout = stream_get_contents($pipes[1]);
        $stderr = stream_get_contents($pipes[2]);
        /* proc_close closes the pipe resources. Do not close them here. */
        $exitCode = proc_close($process);

        return [(string) $stdout . (string) $stderr, $exitCode];
    }

    private function findAutoloadPath(): ?string
    {
        if (class_exists(\Composer\Autoload\ClassLoader::class)) {
            $file = (new ReflectionClass(\Composer\Autoload\ClassLoader::class))->getFileName();
            if (is_string($file)) {
                $autoload = dirname($file, 2) . '/autoload.php';
                if (is_file($autoload)) {
                    return $autoload;
                }
            }
        }

        foreach (get_included_files() as $file) {
            if (str_ends_with($file, '/vendor/autoload.php')) {
                return $file;
            }
        }

        return null;
    }
}
