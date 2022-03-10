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

namespace Swow\Debug;

use Error;
use SplFileObject;
use Swow\Channel;
use Swow\ChannelException;
use Swow\Coroutine;
use Swow\Debug\Debugger\DebugContext;
use Swow\Errno;
use Swow\Signal;
use Swow\Socket;
use Swow\Util\Handler;
use Throwable;
use WeakMap;
use function array_filter;
use function array_shift;
use function array_sum;
use function basename;
use function bin2hex;
use function count;
use function ctype_print;
use function end;
use function explode;
use function extension_loaded;
use function file_exists;
use function file_get_contents;
use function func_get_args;
use function getenv;
use function implode;
use function is_a;
use function is_array;
use function is_bool;
use function is_float;
use function is_int;
use function is_null;
use function is_numeric;
use function is_object;
use function is_string;
use function json_decode;
use function max;
use function mb_strlen;
use function mb_substr;
use function rtrim;
use function sprintf;
use function str_repeat;
use function str_replace;
use function strlen;
use function substr;
use function trim;
use function usleep;
use const JSON_THROW_ON_ERROR;
use const PHP_INT_MAX;

class Debugger
{
    public const SDB = <<<'TEXT'
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
TEXT;

    protected const SOURCE_FILE_CONTENT_PADDING = 4;
    protected const SOURCE_FILE_DEFAULT_LINE_COUNT = 8;

    /** @var static */
    protected static self $instance;

    protected static ?bool $useMbString = null;

    protected static WeakMap $coroutineDebugWeakMap;

    protected Socket $input;

    protected Socket $output;

    protected Socket $error;

    protected bool $daemon = true;

    protected bool $reloading = false;

    /** @var callable|null */
    protected $sourcePositionHandler;

    protected string $lastCommand = '';

    protected Coroutine $currentCoroutine;

    protected int $currentFrameIndex = 0;

    protected SplFileObject $currentSourceFile;

    protected int $currentSourceFileLine = 0;

    protected ?Handler $breakPointHandler = null;

    /** @var string[] */
    protected array $breakPoints = [];

    /**  @var int For "next" command, only break when the trace depth
     * is less or equal to the last trace depth */
    protected int $lastTraceDepth = PHP_INT_MAX;

    final public static function getInstance(): static
    {
        /* @phpstan-ignore-next-line */
        return self::$instance ?? (self::$instance = new static());
    }

    protected function __construct()
    {
        $this->input = new Socket(Socket::TYPE_STDIN);
        $this->output = new Socket(Socket::TYPE_STDOUT);
        $this->error = new Socket(Socket::TYPE_STDERR);
        $this
            ->setCurrentCoroutine(Coroutine::getCurrent())
            ->checkPathMap();
    }

    public function __destruct()
    {
        if ($this->breakPointHandler) {
            $this->breakPointHandler->remove();
            $this->breakPointHandler = null;
        }
    }

    public function in(bool $prefix = true): string
    {
        if ($prefix) {
            $this->out("\r> ", false);
        }

        return $this->input->recvString();
    }

    public function out(string $string = '', bool $newline = true): static
    {
        $this->output->write([$string, $newline ? "\n" : null]);

        return $this;
    }

    public function exception(string $string = '', bool $newline = true): static
    {
        $this->output->write([$string, $newline ? "\n" : null]);

        return $this;
    }

    public function error(string $string = '', bool $newline = true): static
    {
        $this->error->write([$string, $newline ? "\n" : null]);

        return $this;
    }

    public function cr(): static
    {
        return $this->out("\r", false);
    }

    public function lf(): static
    {
        return $this->out("\n", false);
    }

    public function clear(): static
    {
        $this->output->sendString("\033c");

        return $this;
    }

    /**
     * @param array<mixed> $table
     */
    public function table(array $table, bool $newline = true): static
    {
        return $this->out($this::tableFormat($table), $newline);
    }

    public function getLastCommand(): string
    {
        return $this->lastCommand;
    }

    protected function callSourcePositionHandler(string $sourcePosition): string
    {
        $sourcePositionHandler = $this->sourcePositionHandler ?? null;
        if ($sourcePositionHandler !== null) {
            return $sourcePositionHandler($sourcePosition);
        }

        return $sourcePosition;
    }

    public function setLastCommand(string $lastCommand): static
    {
        $this->lastCommand = $lastCommand;

        return $this;
    }

    public function setSourcePositionHandler(?callable $sourcePositionHandler): static
    {
        $this->sourcePositionHandler = $sourcePositionHandler;

        return $this;
    }

    public function checkPathMap(): static
    {
        $pathMap = getenv('SDB_PATH_MAP');
        if (is_string($pathMap) && file_exists($pathMap)) {
            $pathMap = json_decode(file_get_contents($pathMap), true, 512, JSON_THROW_ON_ERROR);
            if ((is_countable($pathMap) ? count($pathMap) : 0) > 0) {
                /* This can help you to see the real source position in the host machine in the terminal */
                $this->setSourcePositionHandler(static function (string $sourcePosition) use ($pathMap): string {
                    $search = $replace = [];
                    foreach ($pathMap as $key => $value) {
                        $search[] = $key;
                        $replace[] = $value;
                    }

                    return str_replace($search, $replace, $sourcePosition);
                });
            }
        }

        return $this;
    }

    protected function setCursorVisibility(bool $bool): static
    {
        // TODO tty check support
        /* @phpstan-ignore-next-line */
        if (1 /* is tty */) {
            $this->out("\033[?25" . ($bool ? 'h' : 'l'));
        }

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

    /** @return WeakMap<Coroutine, DebugContext> */
    public static function getCoroutineDebugWeakMap(): WeakMap
    {
        return static::$coroutineDebugWeakMap ?? (static::$coroutineDebugWeakMap = new WeakMap());
    }

    public static function getDebugContextOfCoroutine(Coroutine $coroutine): DebugContext
    {
        return static::getCoroutineDebugWeakMap()[$coroutine] ??= new DebugContext();
    }

    public function getCurrentFrameIndex(): int
    {
        return $this->currentFrameIndex;
    }

    public function getCurrentFrameIndexExtended(): int
    {
        return $this->currentFrameIndex + static::getExtendedLevelOfCoroutine($this->getCurrentCoroutine());
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

    protected static function hasMbString(): bool
    {
        if (static::$useMbString === null) {
            static::$useMbString = extension_loaded('mbstring');
        }

        return static::$useMbString;
    }

    protected static function strlen(string $string): int
    {
        if (!static::hasMbString()) {
            return strlen($string);
        }

        return mb_strlen($string);
    }

    protected static function substr(string $string, int $offset, ?int $length = null): string
    {
        if (!static::hasMbString()) {
            return substr($string, $offset, $length);
        }

        return mb_substr($string, $offset, $length);
    }

    protected static function getMaxLengthOfStringLine(string $string): int
    {
        $maxLength = 0;
        $lines = explode("\n", $string);
        foreach ($lines as $line) {
            $maxLength = max($maxLength, static::strlen($line));
        }

        return $maxLength;
    }

    /**
     * @param array<mixed> $table
     */
    protected static function tableFormat(array $table): string
    {
        if (empty($table)) {
            return 'No more content';
        }
        $colLengthMap = [];
        foreach ($table as $item) {
            foreach ($item as $key => $value) {
                $key = (string) $key;
                $value = static::convertValueToString($value, false);
                $colLengthMap[$key] = max(
                    $colLengthMap[$key] ?? 0,
                    static::getMaxLengthOfStringLine($key),
                    static::getMaxLengthOfStringLine($value)
                );
            }
            unset($value);
        }
        // TODO: support \n in keys and values
        $spiltLine = str_repeat('-', array_sum($colLengthMap) + (count($colLengthMap) * 3) + 1);
        $spiltBoldLine = str_replace('-', '=', $spiltLine);
        $result = $spiltLine;
        $result .= "\n";
        $result .= '|';
        foreach ($colLengthMap as $key => $colLength) {
            $result .= ' ' . sprintf("%-{$colLength}s", $key) . ' |';
        }
        $result .= "\n";
        $result .= $spiltBoldLine;
        $result .= "\n";
        foreach ($table as $item) {
            $result .= '|';
            foreach ($item as $key => $value) {
                $value = static::convertValueToString($value, false);
                $result .= ' ' . sprintf("%-{$colLengthMap[$key]}s", $value) . ' |';
            }
            $result .= "\n";
        }
        $result .= $spiltLine;

        return $result;
    }

    protected static function convertValueToString(mixed $value, bool $forArgs = true): string
    {
        switch (true) {
            case is_int($value):
            case is_float($value):
                return (string) $value;
            case is_null($value):
                return 'null';
            case is_bool($value):
                return $value ? 'true' : 'false';
            case is_string($value):
            {
                // TODO: how to display binary content?
                // if (!ctype_print($value)) {
                //     $value = bin2hex($value);
                // }
                $maxLength = $forArgs ? 8 : 512;
                if (static::strlen($value) > $maxLength) {
                    $value = static::substr($value, 0, $maxLength) . '...';
                }
                if ($forArgs) {
                    $value = "'{$value}'";
                }

                return $value;
            }
            case is_array($value):
                if (empty($value)) {
                    return '[]';
                }

                return '[...]';
            case is_object($value):
                return $value::class . '{}';
            default:
                return '...';
        }
    }

    /**
     * @param array{
     *     'function': string|null,
     *     'class': string|null,
     *     'args': array<string>|null,
     * } $frame
     */
    protected static function getExecutingFromFrame(array $frame): string
    {
        $atFunction = $frame['function'] ?? '';
        if ($atFunction) {
            $atClass = $frame['class'] ?? '';
            $delimiter = $atClass ? '::' : '';
            $args = $frame['args'] ?? [];
            $argsString = '';
            if ($args) {
                foreach ($args as $argValue) {
                    $argsString .= static::convertValueToString($argValue) . ', ';
                }
                $argsString = rtrim($argsString, ', ');
            }
            $executing = "{$atClass}{$delimiter}{$atFunction}({$argsString})";
        } else {
            $executing = 'Unknown';
        }

        return $executing;
    }

    /**
     * @param array<int, array{
     *     'function': string|null,
     *     'class': string|null,
     *     'args': array<string>|null,
     *     'file': string|null,
     *     'line': int|null,
     * }> $trace
     * @return array<array{
     *     'frame': int,
     *     'executing': string,
     *     'source position': string,
     * }>
     */
    protected static function convertTraceToTable(array $trace, ?int $frameIndex = null): array
    {
        $traceTable = [];
        foreach ($trace as $index => $frame) {
            if ($frameIndex !== null && $index !== $frameIndex) {
                continue;
            }
            $executing = static::getExecutingFromFrame($frame);
            $file = $frame['file'] ?? null;
            $line = $frame['line'] ?? '?';
            if ($file === null) {
                $sourcePosition = '<internal space>';
            } else {
                $sourcePosition = "{$file}({$line})";
            }
            $traceTable[] = [
                'frame' => $index,
                'executing' => $executing,
                'source position' => $sourcePosition,
            ];
            if ($frameIndex !== null) {
                break;
            }
        }
        if (empty($traceTable)) {
            throw new DebuggerException('No trace info');
        }

        return $traceTable;
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
    protected function showTrace(array $trace, ?int $frameIndex = null, bool $newLine = true): static
    {
        $traceTable = $this::convertTraceToTable($trace, $frameIndex);
        foreach ($traceTable as &$traceItem) {
            $traceItem['source position'] = $this->callSourcePositionHandler($traceItem['source position']);
        }
        unset($traceItem);
        $this->table($traceTable, $newLine);

        return $this;
    }

    protected static function getStateNameOfCoroutine(Coroutine $coroutine): string
    {
        if (static::getDebugContextOfCoroutine($coroutine)->stopped) {
            $state = 'stopped';
        } else {
            $state = $coroutine->getStateName();
        }

        return $state;
    }

    protected static function getExtendedLevelOfCoroutine(Coroutine $coroutine): int
    {
        if (static::getDebugContextOfCoroutine($coroutine)->stopped) {
            // skip extended statement handler
            $level = static::getCoroutineTraceDiffLevel($coroutine, 'getExtendedLevelOfCoroutine');
        } else {
            $level = 0;
        }

        return $level;
    }

    /**
     * @return array<int, array{
     *     'function': string|null,
     *     'class': string|null,
     *     'args': array<string>,
     *     'file': string|null,
     *     'line': int|null,
     *     'type': string|null,
     * }>|array{
     *     'function': string|null,
     *     'class': string|null,
     *     'args': array<string>,
     *     'file': string|null,
     *     'line': int|null,
     *     'type': string|null,
     * } $trace
     * @psalm-return ($index is null ? array<int, array{
     *     'function': string|null,
     *     'class': string|null,
     *     'args': array<string>,
     *     'file': string|null,
     *     'line': int|null,
     *     'type': string|null,
     * }> : array{
     *     'function': string|null,
     *     'class': string|null,
     *     'args': array<string>,
     *     'file': string|null,
     *     'line': int|null,
     *     'type': string|null,
     * }) $trace
     */
    protected static function getTraceOfCoroutine(Coroutine $coroutine, ?int $index = null): array
    {
        $level = static::getExtendedLevelOfCoroutine($coroutine);
        if ($index !== null) {
            $level += $index;
            $limit = 1;
        } else {
            $limit = 0;
        }
        $trace = $coroutine->getTrace($level, $limit);
        if ($index !== null) {
            $trace = $trace[0] ?? [];
        }

        return $trace;
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
        return $this::getTraceOfCoroutine($this->getCurrentCoroutine());
    }

    /**
     * @return array{
     *     'id': int,
     *     'state': string,
     *     'round': int,
     *     'elapsed': string,
     *     'executing': string|null,
     *     'source position': string|null,
     * } $simpleInfo
     * @psalm-return ($whatAreYouDoing is true ?array{
     *     'id': int,
     *     'state': string,
     *     'round': int,
     *     'elapsed': string,
     *     'executing': string,
     *     'source position': string,
     * } : array{
     *     'id': int,
     *     'state': string,
     *     'round': int,
     *     'elapsed': string,
     * }) $simpleInfo
     */
    protected static function getSimpleInfoOfCoroutine(Coroutine $coroutine, bool $whatAreYouDoing): array
    {
        $info = [
            'id' => $coroutine->getId(),
            'state' => static::getStateNameOfCoroutine($coroutine),
            'round' => $coroutine->getRound(),
            'elapsed' => $coroutine->getElapsedAsString(),
        ];
        if ($whatAreYouDoing) {
            $frame = static::getTraceOfCoroutine($coroutine, 0);
            $info['executing'] = static::getExecutingFromFrame($frame);
            $file = $frame['file'] ?? null;
            $line = $frame['line'] ?? 0;
            if ($file === null) {
                $sourcePosition = '<internal space>';
            } else {
                $file = basename($file);
                $sourcePosition = "{$file}({$line})";
            }
            $info['source position'] = $sourcePosition;
        }

        return $info;
    }

    protected function showCoroutine(Coroutine $coroutine, bool $newLine = true): static
    {
        $debugInfo = static::getSimpleInfoOfCoroutine($coroutine, false);
        $trace = $this::getTraceOfCoroutine($coroutine);
        $this->table([$debugInfo], !$trace);
        if ($trace) {
            $this->cr()->showTrace($trace, null, $newLine);
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
            $info['source position'] = $this->callSourcePositionHandler($info['source position']);
            $map[] = $info;
        }

        return $this->table($map);
    }

    /**
     * @return array<array{
     *     'line': string|int,
     *     'content': string,
     * }>
     * @phpstan-return array<array{
     *     'line': string|positive-int,
     *     'content': string,
     * }>
     * @psalm-return array<array{
     *     'line': string|positive-int,
     *     'content': string,
     * }>
     */
    protected static function getSourceFileContentAsTable(
        string $filename,
        int $line,
        ?SplFileObject &$sourceFile = null,
        int $lineCount = self::SOURCE_FILE_DEFAULT_LINE_COUNT,
    ): array {
        $sourceFile = null;
        if ($line < 2) {
            $startLine = $line;
        } else {
            $startLine = $line - ($lineCount - static::SOURCE_FILE_CONTENT_PADDING - 1);
        }
        $file = new SplFileObject($filename);
        $sourceFile = $file;
        $i = 0;
        while (!$file->eof()) {
            $lineContent = $file->fgets();
            $i++;
            if ($i === $startLine - 1) {
                break;
            }
        }
        if (!isset($lineContent)) {
            throw new DebuggerException('File Line not found');
        }
        $content = [];
        for ($i++; $i < $startLine + $lineCount; $i++) {
            if ($file->eof()) {
                break;
            }
            $lineContent = $file->fgets();
            $content[] = [
                'line' => $i === $line ? "{$i}->" : $i,
                'content' => rtrim($lineContent),
            ];
        }

        return $content;
    }

    /**
     * @param array<array{
     *     'file': string|null,
     *     'line': int|null,
     * }> $trace
     * @return array<array{
     *     'line': string,
     *     'content': string,
     * }>
     */
    protected static function getSourceFileContentByTrace(array $trace, int $frameIndex, ?SplFileObject &$sourceFile = null, ?int &$sourceFileLine = null): array
    {
        /* init */
        $sourceFile = null;
        $sourceFileLine = 0;
        /* get frame info */
        $frame = $trace[$frameIndex] ?? null;
        if (!$frame || empty($frame['file']) || !isset($frame['line'])) {
            return [];
        }
        $file = $frame['file'];
        if (!file_exists($file)) {
            throw new DebuggerException('Source File not found');
        }
        $line = $frame['line'];
        if (!is_numeric($line)) {
            throw new DebuggerException('Invalid source file line no');
        }
        $line = (int) $line;
        // $class = $frame['class'] ?? '';
        // $function = $frame['function'] ?? '';
        // if (is_a($class, self::class, true) && $function === 'breakPointHandler') {
        //     $line -= 1;
        // }
        $sourceFileLine = $line;

        return static::getSourceFileContentAsTable($file, $line, $sourceFile);
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

        return $this->table($contentTable);
    }

    /**
     * @return array<array{
     *     'line': int,
     *     'content': string,
     * }>
     */
    protected static function getFollowingSourceFileContent(
        SplFileObject $sourceFile,
        int $startLine,
        int $lineCount = self::SOURCE_FILE_DEFAULT_LINE_COUNT,
        int $offset = self::SOURCE_FILE_CONTENT_PADDING
    ): array {
        $content = [];
        for ($i = $startLine + $offset + 1; $i < $startLine + $offset + $lineCount; $i++) {
            if ($sourceFile->eof()) {
                break;
            }
            $content[] = [
                'line' => $i,
                'content' => rtrim($sourceFile->fgets()),
            ];
        }

        return $content;
    }

    protected function showFollowingSourceFileContent(int $lineCount = self::SOURCE_FILE_DEFAULT_LINE_COUNT): static
    {
        $sourceFile = $this->getCurrentSourceFile();
        if (!$sourceFile) {
            throw new DebuggerException('No source file was selected');
        }
        $line = $this->getCurrentSourceFileLine();

        return $this
            ->table($this::getFollowingSourceFileContent($sourceFile, $line, $lineCount))
            ->setCurrentSourceFileLine($line + $lineCount - 1);
    }

    /** We need to subtract the number of call stack layers of the Debugger itself */
    protected static function getCoroutineTraceDiffLevel(Coroutine $coroutine, string $name): int
    {
        static $diffLevelCache = [];
        if (isset($diffLevelCache[$name])) {
            return $diffLevelCache[$name];
        }

        $trace = $coroutine->getTrace();
        $diffLevel = 0;
        foreach ($trace as $index => $frame) {
            $class = $frame['class'] ?? '';
            if (is_a($class, self::class, true)) {
                $diffLevel = $index;
            }
        }
        /* Debugger::breakPointHandler() or something like it are not the Debugger frame,
         * but we do not need to -1 here because index is start with 0. */
        if ($coroutine === Coroutine::getCurrent()) {
            $diffLevel -= 1; /* we are in getTrace() here */
        }

        return $diffLevelCache[$name] = $diffLevel;
    }

    public function addBreakPoint(string $point): static
    {
        $this
            ->checkBreakPointHandler()
            ->breakPoints[] = $point;

        return $this;
    }

    public static function break(): void
    {
        $coroutine = Coroutine::getCurrent();
        $context = static::getDebugContextOfCoroutine($coroutine);
        $context->stopped = true;
        Coroutine::yield();
        $context->stopped = false;
    }

    public static function breakOn(string $point): static
    {
        return static::getInstance()->addBreakPoint($point);
    }

    protected static function breakPointHandler(): void
    {
        $debugger = static::getInstance();
        $coroutine = Coroutine::getCurrent();
        $context = static::getDebugContextOfCoroutine($coroutine);

        if ($context->stop) {
            $traceDepth = $coroutine->getTraceDepth();
            $traceDiffLevel = static::getCoroutineTraceDiffLevel($coroutine, 'breakPointHandler');
            if ($traceDepth - $traceDiffLevel <= $debugger->lastTraceDepth) {
                static::break();
            }
            return;
        }

        $file = $coroutine->getExecutedFilename(2);
        $line = $coroutine->getExecutedLineno(2);
        $fullPosition = "{$file}:{$line}";
        $basename = basename($file);
        $basePosition = "{$basename}:{$line}";
        $function = $coroutine->getExecutedFunctionName(3);
        $baseFunction = $function;
        if (str_contains($function, '\\')) {
            $baseFunction = explode('\\', $function);
            $baseFunction = end($baseFunction);
        }
        if (str_contains($function, '::')) {
            $baseFunction = explode('::', $function);
            $baseFunction = end($baseFunction);
        }
        $hit = false;
        $breakPoints = $debugger->breakPoints;
        foreach ($breakPoints as $breakPoint) {
            if (
                $breakPoint === $basePosition ||
                $breakPoint === $baseFunction ||
                $breakPoint === $function ||
                $breakPoint === $fullPosition
            ) {
                $debugger->out("Hit breakpoint <{$breakPoint}> on Coroutine#{$coroutine->getId()}");
                $hit = true;
                break;
            }
        }
        if ($hit) {
            $context->stop = true;
            static::break();
        }
    }

    protected function checkBreakPointHandler(): static
    {
        $this->breakPointHandler ??= registerExtendedStatementHandler([$this, 'breakPointHandler']);

        return $this;
    }

    protected function waitStoppedCoroutine(Coroutine $coroutine): void
    {
        $context = static::getDebugContextOfCoroutine($coroutine);
        if ($context->stopped) {
            return;
        }
        $signalChannel = new Channel();
        $signalListener = Coroutine::run(static function () use ($signalChannel): void {
            // Always wait signal int, prevent signals from coming in gaps
            Signal::wait(Signal::INT);
            $signalChannel->push(true);
        });
        /* @noinspection PhpConditionAlreadyCheckedInspection */
        do {
            try {
                // this will yield out from current coroutine,
                // $context->stopped may be changed here
                $signalChannel->pop(100);
                throw new DebuggerException('Cancelled');
            } catch (ChannelException $exception) {
                // if timed out, continue
                if ($exception->getCode() !== Errno::ETIMEDOUT) {
                    throw $exception;
                }
            }
            /* @phpstan-ignore-next-line */
        } while (!$context->stopped);
        /* @phpstan-ignore-next-line */
        $signalListener->kill();
    }

    protected static function isAlone(): bool
    {
        foreach (Coroutine::getAll() as $coroutine) {
            if ($coroutine === Coroutine::getCurrent()) {
                continue;
            }

            return false;
        }

        return true;
    }

    public function logo(): static
    {
        return $this->clear()->out($this::SDB)->lf();
    }

    public function run(string $keyword = ''): static
    {
        if ($this->reloading) {
            $this->reloading = false;
            goto _recvLoop;
        }
        $this->setCursorVisibility(true);
        if (static::isAlone()) {
            $this->daemon = false;
            $this->logo()->out('Enter \'r\' to run your program');
            goto _recvLoop;
        }
        if ($keyword !== '') {
            $this->lf()->out("You can input '{$keyword}' to to call out the debug interface...");
        }
        _restart:
        if ($keyword !== '') {
            while ($in = $this->in(false)) {
                if (trim($in) === $keyword) {
                    break;
                }
            }
        }
        $this->logo();
        _recvLoop:
        while ($in = $this->in()) {
            if ($in === "\n") {
                $in = $this->getLastCommand();
            }
            $this->setLastCommand($in);
            _next:
            try {
                $lines = array_filter(explode("\n", $in));
                foreach ($lines as $line) {
                    $arguments = explode(' ', $line);
                    foreach ($arguments as &$argument) {
                        $argument = trim($argument);
                    }
                    unset($argument);
                    $arguments = array_filter($arguments, static fn (string $value) => $value !== '');
                    $command = array_shift($arguments);
                    switch ($command) {
                        case 'ps':
                            $this->showCoroutines(Coroutine::getAll());
                            break;
                        case 'attach':
                        case 'co':
                        case 'coroutine':
                            $id = $arguments[0] ?? 'unknown';
                            if (!is_numeric($id)) {
                                throw new DebuggerException('Argument[1]: Coroutine id must be numeric');
                            }
                            $coroutine = Coroutine::get((int) $id);
                            if (!$coroutine) {
                                throw new DebuggerException("Coroutine#{$id} Not found");
                            }
                            if ($command === 'attach') {
                                $this->checkBreakPointHandler();
                                if ($coroutine === Coroutine::getCurrent()) {
                                    throw new DebuggerException('Attach debugger is not allowed');
                                }
                                static::getDebugContextOfCoroutine($coroutine)->stop = true;
                            }
                            $this->setCurrentCoroutine($coroutine);
                            $in = 'bt';
                            goto _next;
                        case 'bt':
                        case 'backtrace':
                            $this->showCoroutine($this->getCurrentCoroutine(), false)
                                ->showSourceFileContentByTrace($this->getCurrentCoroutineTrace(), 0, true);
                            break;
                        case 'f':
                        case 'frame':
                            $frameIndex = $arguments[0] ?? null;
                            if (!is_numeric($frameIndex)) {
                                throw new DebuggerException('Frame index must be numeric');
                            }
                            $frameIndex = (int) $frameIndex;
                            if ($this->getCurrentFrameIndex() !== $frameIndex) {
                                $this->out("Switch to frame {$frameIndex}");
                            }
                            $this->setCurrentFrameIndex($frameIndex);
                            $trace = $this->getCurrentCoroutineTrace();
                            $frameIndex = $this->getCurrentFrameIndex();
                            $this
                                ->showTrace($trace, $frameIndex, false)
                                ->showSourceFileContentByTrace($trace, $frameIndex, true);
                            break;
                        case 'b':
                        case 'breakpoint':
                            $breakPoint = $arguments[0] ?? '';
                            if ($breakPoint === '') {
                                throw new DebuggerException('Invalid break point');
                            }
                            $coroutine = $this->getCurrentCoroutine();
                            if ($coroutine === Coroutine::getCurrent()) {
                                $this
                                    ->out("Added global break-point <{$breakPoint}>")
                                    ->addBreakPoint($breakPoint);
                            }
                            break;
                        case 'n':
                        case 'next':
                        case 's':
                        case 'step':
                        case 'c':
                        case 'continue':
                            $coroutine = $this->getCurrentCoroutine();
                            $context = static::getDebugContextOfCoroutine($coroutine);
                            if (!$context->stopped) {
                                if ($context->stop) {
                                    $this->waitStoppedCoroutine($coroutine);
                                } else {
                                    throw new DebuggerException('Not in debugging');
                                }
                            }
                            switch ($command) {
                                case 'n':
                                case 'next':
                                case 's':
                                case 'step_in':
                                    if ($command === 'n' || $command === 'next') {
                                        $this->lastTraceDepth = $coroutine->getTraceDepth() - static::getCoroutineTraceDiffLevel($coroutine, 'nextCommand');
                                    }
                                    $coroutine->resume();
                                    $this->waitStoppedCoroutine($coroutine);
                                    $this->lastTraceDepth = PHP_INT_MAX;
                                    $in = 'f 0';
                                    goto _next;
                                case 'c':
                                case 'continue':
                                    static::getDebugContextOfCoroutine($coroutine)->stop = false;
                                    $this->out("Coroutine#{$coroutine->getId()} continue to run...");
                                    $coroutine->resume();
                                    break;
                                default:
                                    throw new Error('Never here');
                            }
                            break;
                        case 'l':
                        case 'list':
                            $lineCount = $arguments[0] ?? null;
                            if ($lineCount === null) {
                                $this->showFollowingSourceFileContent();
                            } elseif (is_numeric($lineCount)) {
                                $this->showFollowingSourceFileContent((int) $lineCount);
                            } else {
                                throw new DebuggerException('Argument[1]: line no must be numeric');
                            }
                            break;
                        case 'p':
                        case 'print':
                        case 'exec':
                            $expression = implode(' ', $arguments);
                            if (!$expression) {
                                throw new DebuggerException('No expression');
                            }
                            if ($command === 'exec') {
                                $transfer = new Channel();
                                Coroutine::run(static function () use ($expression, $transfer): void {
                                    $transfer->push(Coroutine::getCurrent()->eval($expression));
                                });
                                // TODO: support ctrl + c (also support ctrl + c twice confirm on global scope?)
                                $result = var_dump_return($transfer->pop());
                            } else {
                                $coroutine = $this->getCurrentCoroutine();
                                $index = $this->getCurrentFrameIndexExtended();
                                $result = var_dump_return($coroutine->eval($expression, $index));
                            }
                            $this->out($result, false);
                            break;
                        case 'vars':
                            $coroutine = $this->getCurrentCoroutine();
                            $index = $this->getCurrentFrameIndexExtended();
                            $result = var_dump_return($coroutine->getDefinedVars($index));
                            $this->out($result, false);
                            break;
                        case 'z':
                        case 'zombie':
                        case 'zombies':
                            $time = $arguments[0] ?? null;
                            if (!is_numeric($time)) {
                                throw new DebuggerException('Argument[1]: Time must be numeric');
                            }
                            $this->out("Scanning zombie coroutines ({$time}s)...");
                            $roundMap = [];
                            foreach (Coroutine::getAll() as $coroutine) {
                                $roundMap[$coroutine->getId()] = $coroutine->getRound();
                            }
                            usleep((int) ($time * 1000 * 1000));
                            $coroutines = Coroutine::getAll();
                            $zombies = [];
                            foreach ($roundMap as $id => $round) {
                                $coroutine = $coroutines[$id] ?? null;
                                if (!$coroutine) {
                                    continue;
                                }
                                if ($coroutine->getRound() === $round) {
                                    $zombies[] = $coroutine;
                                }
                            }
                            $this
                                ->out('Following coroutine maybe zombies:')
                                ->showCoroutines($zombies);
                            break;
                        case 'kill':
                            if (count($arguments) === 0) {
                                throw new DebuggerException('Required coroutine id');
                            }
                            foreach ($arguments as $index => $argument) {
                                if (!is_numeric($argument)) {
                                    $this->exception("Argument[{$index}] '{$argument}' is not numeric");
                                }
                            }
                            foreach ($arguments as $argument) {
                                $coroutine = Coroutine::get((int) $argument);
                                if ($coroutine) {
                                    $coroutine->kill();
                                    $this->out("Coroutine#{$argument} killed");
                                } else {
                                    $this->exception("Coroutine#{$argument} not exists");
                                }
                            }
                            break;
                        case 'killall':
                            Coroutine::killAll();
                            $this->out('All coroutines has been killed');
                            break;
                        case 'clear':
                            $this->clear();
                            break;
                        case 'q':
                        case 'quit':
                        case 'exit':
                            $this->clear();
                            if ($keyword !== '' && !static::isAlone()) {
                                /* we can input keyword to call out the debugger later */
                                goto _restart;
                            }
                            goto _quit;
                        case 'r':
                        case 'run':
                            if ($this->daemon) {
                                throw new DebuggerException('Debugger is already running');
                            }
                            $args = func_get_args();
                            Coroutine::run(function () use ($args): void {
                                $this->reloading = true;
                                $this->daemon = true;
                                $this
                                    ->out('Program is running...')
                                    ->run(...$args);
                            });
                            goto _quit;
                        case null:
                            break;
                        default:
                            if (!ctype_print($command)) {
                                $command = bin2hex($command);
                            }
                            throw new DebuggerException("Unknown command '{$command}'");
                    }
                }
            } catch (DebuggerException $exception) {
                $this->exception($exception->getMessage());
            } catch (Throwable $throwable) {
                $this->error((string) $throwable);
            }
        }

        _quit:
        return $this;
    }

    public static function runOnTTY(string $keyword = 'sdb'): static
    {
        return static::getInstance()->run($keyword);
    }
}
