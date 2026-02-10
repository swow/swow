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

namespace Swow\Debug\Debugger;

use ReflectionObject;
use SplFileObject;
use Swow\Coroutine;
use Throwable;

use function array_slice;
use function count;

use const PHP_INT_MAX;

class Debugger
{
    protected const DEFAULT_GREETING = <<<'GREETING'
  ██████ ▓█████▄  ▄▄▄▄
▒██    ▒ ▒██▀ ██▌▓█████▄
░ ▓██▄   ░██   █▌▒██▒ ▄██
  ▒   ██▒░▓█▄   ▌▒██░█▀
▒██████▒▒░▒████▓ ░▓█  ▀█▓
 ░▒▓▒ ▒ ░ ▒▒▓  ▒ ░▒▓███▀▒
  ░▒      ░ ▒  ▒ ▒░▒   ░
-------------------
SDB (Swow Debugger)
-------------------

GREETING;

    /** @var static */
    protected static self $instance;

    public Coroutine $debuggerCoroutine;

    protected ReflectionObject $reflection;

    /** @var string[] */
    protected array $lastCommand = [];

    protected Coroutine $currentCoroutine;

    protected int $currentFrameIndex = 0;

    protected ?SplFileObject $currentSourceFile = null;

    protected int $currentSourceFileLine = 0;

    /**  @var int For "next" command, only break when the trace depth
     * is less or equal to the last trace depth */
    protected int $lastTraceDepth = PHP_INT_MAX;

    use DebuggerBreakpointTrait;

    use DebuggerCommandTrait;

    use DebuggerDebugContextTrait;

    use DebuggerIoTrait;

    use DebuggerSourceMapTrait;

    use DebuggerStaticGetterTrait;

    final protected static function getInstance(): static
    {
        return self::$instance ?? throw new DebuggerException('Debugger is not initialized');
    }

    protected function __construct(DebuggerIoInterface $io)
    {
        $this->reflection = new ReflectionObject($this);
        $this->__constructDebuggerIo($io);
        $this->__constructDebuggerSourceMap();
        $this->setCurrentCoroutine(Coroutine::getCurrent());
    }

    public function __destruct()
    {
        $this->__destructDebuggerBreakpoint();
    }

    /** @return string[] */
    public function getLastCommand(): array
    {
        return $this->lastCommand;
    }

    /** @param string[] $lastCommand */
    public function setLastCommand(array $lastCommand): static
    {
        $this->lastCommand = $lastCommand;

        return $this;
    }

    public function getCurrentCoroutine(): Coroutine
    {
        return $this->currentCoroutine;
    }

    protected function setCurrentCoroutine(Coroutine $coroutine): static
    {
        $this->currentCoroutine = $coroutine;
        $this->currentFrameIndex = 0;

        return $this;
    }

    /**
     * @return array<int, array{
     *     'function': string|null,
     *     'class': string|null,
     *     'args': array<string>,
     *     'file': string|null,
     *     'line': int|null,
     *     'type': string|null,
     * }> $trace
     */
    protected function getCurrentCoroutineTrace(): array
    {
        return static::getTraceOfCoroutine($this->getCurrentCoroutine());
    }

    public function getCurrentFrameIndex(): int
    {
        return $this->currentFrameIndex;
    }

    public function getCurrentFrameIndexExtendedForExecution(): int
    {
        return $this->currentFrameIndex + static::getExtendedLevelOfCoroutineForExecution($this->getCurrentCoroutine());
    }

    protected function setCurrentFrameIndex(int $index): static
    {
        if (count($this->getCurrentCoroutineTrace()) < $index) {
            throw new DebuggerException('Invalid frame index');
        }
        $this->currentFrameIndex = $index;

        return $this;
    }

    public function getCurrentSourceFile(): ?SplFileObject
    {
        return $this->currentSourceFile;
    }

    public function setCurrentSourceFile(?SplFileObject $currentSourceFile): static
    {
        $this->currentSourceFile = $currentSourceFile;

        return $this;
    }

    public function getCurrentSourceFileLine(): int
    {
        return $this->currentSourceFileLine;
    }

    public function setCurrentSourceFileLine(int $currentSourceFileLine): static
    {
        $this->currentSourceFileLine = $currentSourceFileLine;

        return $this;
    }

    /**
     * @param array<int, array{
     *     'function': string|null,
     *     'class': string|null,
     *     'args': array<string>,
     *     'file': string|null,
     *     'line': int|null,
     * }> $trace
     */
    protected function showTrace(array $trace, ?int $frameIndex = null): static
    {
        $traceTable = DebuggerHelper::convertTraceToTable($trace, $frameIndex);
        foreach ($traceTable as &$traceItem) {
            $traceItem['source_position'] = $this->callSourceMapHandler($traceItem['source_position']);
        }
        unset($traceItem);
        $this->table($traceTable);

        return $this;
    }

    protected function showCoroutine(Coroutine $coroutine): static
    {
        $debugInfo = static::getSimpleInfoOfCoroutine($coroutine, false);
        $trace = static::getTraceOfCoroutine($coroutine);
        $this->table([$debugInfo]);
        if ($trace) {
            $this->cr()->showTrace($trace);
        } else {
            $this->lf();
        }

        return $this;
    }

    /**
     * @param Coroutine[] $coroutines
     */
    public function showCoroutines(array $coroutines): static
    {
        $map = [];
        foreach ($coroutines as $coroutine) {
            if ($coroutine === Coroutine::getCurrent()) {
                continue;
            }
            $info = static::getSimpleInfoOfCoroutine($coroutine, true);
            $info['source_position'] = $this->callSourceMapHandler($info['source_position']);
            $map[] = $info;
        }

        return $this->table($map)->lf();
    }

    /**
     * @param array<array{
     *     'file': string|null,
     *     'line': int|null,
     * }> $trace
     */
    public function showSourceFileContentByTrace(array $trace, int $frameIndex, bool $following = false): static
    {
        $file = null;
        $line = 0;
        try {
            $contentTable = $this::getSourceFileContentByTrace($trace, $frameIndex, $file, $line);
        } catch (DebuggerException $exception) {
            $this->lf();
            throw $exception;
        }
        $this
            ->setCurrentSourceFile($file)
            ->setCurrentSourceFileLine($line);
        if (!$contentTable) {
            return $this->lf();
        }
        if ($following) {
            $this->cr();
        }

        return $this->table($contentTable)->lf();
    }

    protected function showFollowingSourceFileContent(int $lineCount = DebuggerHelper::SOURCE_FILE_DEFAULT_LINE_COUNT): static
    {
        $sourceFile = $this->getCurrentSourceFile();
        if (!$sourceFile) {
            throw new DebuggerException('No source file was selected');
        }
        $line = $this->getCurrentSourceFileLine();

        return $this
            ->table(DebuggerHelper::getFollowingSourceFileContent($sourceFile, $line, $lineCount))
            ->lf()
            ->setCurrentSourceFileLine($line + $lineCount - 1);
    }

    // protected static function isNoOtherCoroutinesRunning(): bool
    // {
    //     foreach (Coroutine::getAll() as $coroutine) {
    //         if ($coroutine === Coroutine::getCurrent()) {
    //             continue;
    //         }
    //
    //         return false;
    //     }
    //
    //     return true;
    // }

    protected function loop(): void
    {
        while (true) {
            $in = $this->in();
            if ($in === []) {
                $in = $this->getLastCommand();
                if ($in === []) {
                    continue;
                }
            }
            $this->setLastCommand($in);
            try {
                $command = $in[0];
                $arguments = array_slice($in, 1);
                switch ($command) {
                    case null:
                        break;
                    default:
                        $this->executeCommand($command, $arguments);
                }
            } catch (DebuggerException $exception) {
                $this->out([$exception->getMessage(), "\n"]);
            } catch (Throwable $throwable) {
                $this->error([(string) $throwable, "\n"]);
            }
        }
    }

    public function run(): static
    {
        if (isset($this->debuggerCoroutine)) {
            throw new DebuggerException('Debugger is already running');
        }
        // $this->logo();
        $this->setCursorVisibility(true);
        $this->debuggerCoroutine = Coroutine::run([$this, 'loop']);
        return $this;
    }

    public static function runOnTTY(): static
    {
        self::$instance = new static(new DebuggerIoStdIO(greeting: static::DEFAULT_GREETING));
        return static::getInstance()->run();
    }

    public static function runOnEofStream(): static
    {
        self::$instance = new static(new DebuggerIoEofStream('127.0.0.1', port: 33284, greeting: static::DEFAULT_GREETING));
        return static::getInstance()->run();
    }
}
