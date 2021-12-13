<?php
/**
 * This file is part of Swow
 *
 * @link     https://github.com/swow/swow
 * @contact  twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

namespace Swow\Debug;

use ArrayObject;
use SplFileObject;
use Swow\Coroutine;
use Swow\Signal;
use Swow\Socket;
use Swow\Util\FileSystem\IOException;
use Throwable;
use WeakMap;
use function array_filter;
use function array_shift;
use function array_sum;
use function assert;
use function basename;
use function bin2hex;
use function count;
use function ctype_print;
use function debug_backtrace;
use function end;
use function explode;
use function extension_loaded;
use function fclose;
use function fgets;
use function file_exists;
use function file_get_contents;
use function fopen;
use function func_get_args;
use function get_class;
use function getenv;
use function implode;
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
use function strpos;
use function substr;
use function Swow\Util\var_dump_return;
use function trigger_error;
use function trim;
use function usleep;
use const DEBUG_BACKTRACE_IGNORE_ARGS;
use const E_USER_WARNING;
use const JSON_THROW_ON_ERROR;
use const PHP_EOL;
use const PHP_VERSION_ID;
use const Swow\Errno\ETIMEDOUT;

class Debugger
{
    public const SDB = <<<'TEXT'
  ____    ____    ____  
 / ___|  |  _ \  | __ ) 
 \___ \  | | | | |  _ \ 
  ___) | | |_| | | |_) |
 |____/  |____/  |____/
TEXT;

    protected const SOURCE_FILE_CONTENT_PADDING = 4;

    protected const SOURCE_FILE_DEFAULT_LINE_COUNT = 8;

    /* @var bool */
    private static $breakPointHandlerRegistered = false;

    /* @var $this */
    protected static $instance;

    /* @var bool */
    protected static $useMbString;

    /* @var WeakMap */
    protected static $coroutineDebugWeakMap;

    /* @var Socket */
    protected $input;

    /* @var Socket */
    protected $output;

    /* @var Socket */
    protected $error;

    /* @var bool */
    protected $reloading;

    /* @var callable */
    protected $sourcePositionHandler;

    /* @var string */
    protected $lastCommand = '';

    /* @var Coroutine */
    protected $currentCoroutine;

    /* @var int */
    protected $currentFrameIndex = 0;

    /* @var SplFileObject */
    protected $currentSourceFile;

    /* @var int */
    protected $currentSourceFileLine = 0;

    /* @var bool */
    protected $breakPointHandlerEnabled = false;

    /* @var string[] */
    protected $breakPoints = [];

    /**
     * @return $this
     */
    final public static function getInstance()
    {
        return self::$instance ?? (self::$instance = new static());
    }

    public function __construct()
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
        if ($this->breakPointHandlerEnabled) {
            self::removeBreakPointHandler();
        }
    }

    public function in(bool $prefix = true): string
    {
        if ($prefix) {
            $this->out("\r> ", false);
        }

        return $this->input->recvString();
    }

    /**
     * @return $this
     */
    public function out(string $string = '', bool $newline = true)
    {
        $this->output->write([$string, $newline ? "\n" : null]);

        return $this;
    }

    /**
     * @return $this
     */
    public function exception(string $string = '', bool $newline = true)
    {
        $this->output->write([$string, $newline ? "\n" : null]);

        return $this;
    }

    /**
     * @return $this
     */
    public function error(string $string = '', bool $newline = true)
    {
        $this->error->write([$string, $newline ? "\n" : null]);

        return $this;
    }

    /**
     * @return $this
     */
    public function cr()
    {
        return $this->out("\r", false);
    }

    /**
     * @return $this
     */
    public function lf()
    {
        return $this->out("\n", false);
    }

    /**
     * @return $this
     */
    public function clear()
    {
        $this->output->sendString("\033c");

        return $this;
    }

    /**
     * @return $this
     */
    public function table(array $table, bool $newline = true)
    {
        return $this->out($this::tableFormat($table), $newline);
    }

    public function getLastCommand(): string
    {
        return $this->lastCommand;
    }

    protected function callSourcePositionHandler(string $sourcePosition): string
    {
        $sourcePositionHandler = $this->sourcePositionHandler;
        if ($sourcePositionHandler !== null) {
            return $sourcePositionHandler($sourcePosition);
        }

        return $sourcePosition;
    }

    /**
     * @return $this
     */
    public function setLastCommand(string $lastCommand)
    {
        $this->lastCommand = $lastCommand;

        return $this;
    }

    /**
     * @return $this
     */
    public function setSourcePositionHandler(callable $sourcePositionHandler)
    {
        $this->sourcePositionHandler = $sourcePositionHandler;

        return $this;
    }

    /**
     * @return $this
     */
    public function checkPathMap()
    {
        $pathMap = getenv('SDB_PATH_MAP');
        if (is_string($pathMap) && file_exists($pathMap)) {
            $pathMap = json_decode(file_get_contents($pathMap), true, 512, JSON_THROW_ON_ERROR);
            if (count($pathMap) > 0) {
                /* This can help you to see the real source position in the host machine in the terminal */
                $this->setSourcePositionHandler(function (string $sourcePosition) use ($pathMap): string {
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

    /**
     * @return $this
     */
    protected function setCursorVisibility(bool $bool)
    {
        if (1 /* is tty */) {
            $this->out("\033[?25" . ($bool ? 'h' : 'l'));
        }

        return $this;
    }

    public function getCurrentCoroutine(): Coroutine
    {
        return $this->currentCoroutine;
    }

    /**
     * @return $this
     */
    protected function setCurrentCoroutine(Coroutine $coroutine)
    {
        $this->currentCoroutine = $coroutine;
        $this->currentFrameIndex = 0;

        return $this;
    }

    public static function getCoroutineDebugWeakMap(): WeakMap
    {
        return static::$coroutineDebugWeakMap ?? (static::$coroutineDebugWeakMap = new WeakMap());
    }

    public static function getDebugFieldOfCoroutine(Coroutine $coroutine, string $field, $defaultValue = null)
    {
        if (PHP_VERSION_ID > 80000) {
            $coroutineDebugContext = static::getCoroutineDebugWeakMap()[$coroutine] ?? [];
            return $coroutineDebugContext[$field] ?? $defaultValue;
        }
        return $coroutine->{$field} ?? $defaultValue;
    }

    public static function setDebugFieldOfCoroutine(Coroutine $coroutine, string $field, $value): void
    {
        if (PHP_VERSION_ID > 80000) {
            $weakMap = static::getCoroutineDebugWeakMap();
            if (!isset($weakMap[$coroutine])) {
                $weakMap[$coroutine] = new ArrayObject();
            }
            $weakMap[$coroutine][$field] = $value;
        } else {
            $coroutine->{$field} = $value;
        }
    }

    public static function unsetDebugFieldOfCoroutine(Coroutine $coroutine, string $field): void
    {
        if (PHP_VERSION_ID > 80000) {
            unset(static::getCoroutineDebugWeakMap()[$coroutine][$field]);
        } else {
            unset($coroutine->{$field});
        }
    }

    public function getCurrentFrameIndex(): int
    {
        return $this->currentFrameIndex;
    }

    public function getCurrentFrameIndexExtended(): int
    {
        return $this->currentFrameIndex + static::getExtendedLevelOfCoroutine($this->getCurrentCoroutine());
    }

    /**
     * @return $this
     */
    protected function setCurrentFrameIndex(int $index)
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

    /**
     * @return $this
     */
    public function setCurrentSourceFile(?SplFileObject $currentSourceFile)
    {
        $this->currentSourceFile = $currentSourceFile;

        return $this;
    }

    public function getCurrentSourceFileLine(): int
    {
        return $this->currentSourceFileLine;
    }

    /**
     * @return $this
     */
    public function setCurrentSourceFileLine(int $currentSourceFileLine)
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

    protected static function substr($string, $offset, $length = null)
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

    protected static function convertValueToString($value, bool $forArgs = true): string
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
                return get_class($value) . '{}';
            default:
                return '...';
        }
    }

    protected static function getExecutingFromFrame(array $frame): string
    {
        $atFunction = $frame['function'] ?? '';
        if ($atFunction) {
            $atClass = $frame['class'] ?? '';
            $delimiter = $atClass ? '::' : '';
            $args = $frame['args'] ?? [];
            $argsString = '';
            if ($args) {
                foreach ($args as $argName => $argValue) {
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
     * @return $this
     */
    protected function showTrace(array $trace, ?int $frameIndex = null, bool $newLine = true)
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
        if (static::getDebugFieldOfCoroutine($coroutine, 'stopped', false)) {
            $state = 'stopped';
        } else {
            $state = $coroutine->getStateName();
        }

        return $state;
    }

    protected static function getExtendedLevelOfCoroutine(Coroutine $coroutine): int
    {
        if (static::getDebugFieldOfCoroutine($coroutine, 'stopped', false)) {
            $level = 2; // skip extended statement handler
        } else {
            $level = 0;
        }

        return $level;
    }

    protected static function getTraceOfCoroutine(Coroutine $coroutine, int $index = null): array
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

    protected function getCurrentCoroutineTrace(): array
    {
        return $this::getTraceOfCoroutine($this->getCurrentCoroutine());
    }

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

    /**
     * @return $this
     */
    protected function showCoroutine(Coroutine $coroutine, bool $newLine = true)
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
     * @return $this
     */
    public function showCoroutines(array $coroutines)
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

    protected static function getSourceFileContentAsTable(string $filename, int $line, SplFileObject &$sourceFile = null, int $lineCount = self::SOURCE_FILE_DEFAULT_LINE_COUNT): array
    {
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

    protected static function getSourceFileContentByTrace(array $trace, int $frameIndex, SplFileObject &$sourceFile = null, int &$sourceFileLine = null): array
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
     * @return $this
     */
    public function showSourceFileContentByTrace(array $trace, int $frameIndex, bool $following = false)
    {
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

    /**
     * @return $this
     */
    protected function showFollowingSourceFileContent(int $lineCount = self::SOURCE_FILE_DEFAULT_LINE_COUNT)
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

    /**
     * @return $this
     */
    public function addBreakPoint(string $point)
    {
        $this
            ->checkBreakPointHandler()
            ->breakPoints[] = $point;

        return $this;
    }

    public static function break(): void
    {
        $coroutine = Coroutine::getCurrent();
        static::setDebugFieldOfCoroutine($coroutine, 'stopped', true);
        Coroutine::yield();
        if (static::getDebugFieldOfCoroutine($coroutine, 'stop', false)) {
            static::setDebugFieldOfCoroutine($coroutine, 'stopped', false);
        } else {
            static::unsetDebugFieldOfCoroutine($coroutine, 'stopped');
        }
    }

    /**
     * @return $this
     */
    public static function breakOn(string $point)
    {
        return static::getInstance()->addBreakPoint($point);
    }

    protected static function breakPointHandler(): void
    {
        $coroutine = Coroutine::getCurrent();

        if (static::getDebugFieldOfCoroutine($coroutine, 'stop', false)) {
            static::break();
            return;
        }

        $file = $coroutine->getExecutedFilename(2);
        $line = $coroutine->getExecutedLineno(2);
        $fullPosition = "{$file}:{$line}";
        $basename = basename($file);
        $basePosition = "{$basename}:{$line}";
        $function = $coroutine->getExecutedFunctionName(3);
        $baseFunction = $function;
        if (strpos($function, '\\') !== false) {
            $baseFunction = explode('\\', $function);
            $baseFunction = end($baseFunction);
        }
        if (strpos($function, '::') !== false) {
            $baseFunction = explode('::', $function);
            $baseFunction = end($baseFunction);
        }
        $hit = false;
        $debugger = self::getInstance();
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
            static::setDebugFieldOfCoroutine($coroutine, 'stop', true);
        }
        if (static::getDebugFieldOfCoroutine($coroutine, 'stop', false)) {
            static::break();
        }
    }

    /**
     * @return $this
     */
    private function checkBreakPointHandler()
    {
        if (!$this->breakPointHandlerEnabled) {
            if (!self::$breakPointHandlerRegistered) {
                $originalHandler = registerExtendedStatementHandler([$this, 'breakPointHandler']);
                if ($originalHandler !== null) {
                    trigger_error('Extended Statement Handler has been overwritten', E_USER_WARNING);
                }
                $this::$breakPointHandlerRegistered = true;
            }
            $this->breakPointHandlerEnabled = true;
        }

        return $this;
    }

    private static function removeBreakPointHandler(): void
    {
        registerExtendedStatementHandler(null);
    }

    protected function waitStoppedCoroutine(Coroutine $coroutine): void
    {
        if (!static::getDebugFieldOfCoroutine($coroutine, 'stopped', false)) {
            $this->out('Waiting...');
        }
        while (!static::getDebugFieldOfCoroutine($coroutine, 'stopped', false)) {
            try {
                Signal::wait(Signal::INT, 100);
                throw new DebuggerException('Cancelled');
            } catch (Signal\Exception $exception) {
                if ($exception->getCode() !== ETIMEDOUT) {
                    throw $exception;
                }
            }
        }
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

    /**
     * @return $this
     */
    public function logo()
    {
        return $this->clear()->out($this::SDB)->lf();
    }

    /**
     * @return $this
     */
    public function run(string $keyword = '')
    {
        if ($this->reloading) {
            $this->reloading = false;
            goto _recvLoop;
        }
        $this->setCursorVisibility(true);
        if (static::isAlone()) {
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
                    $arguments = array_filter($arguments, static function (string $value) {
                        return $value !== '';
                    });
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
                                static::setDebugFieldOfCoroutine($coroutine, 'stop', true);
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
                        case 'c':
                        case 'continue':
                            $coroutine = $this->getCurrentCoroutine();
                            if (!static::getDebugFieldOfCoroutine($coroutine, 'stopped', false)) {
                                if (static::getDebugFieldOfCoroutine($coroutine, 'stop', false)) {
                                    $this->waitStoppedCoroutine($coroutine);
                                }
                                throw new DebuggerException('Not in debugging');
                            }
                            switch ($command) {
                                case 'n':
                                case 'next':
                                    $coroutine->resume();
                                    $this->waitStoppedCoroutine($coroutine);
                                    $in = 'f 0';
                                    goto _next;
                                case 'c':
                                case 'continue':
                                    static::unsetDebugFieldOfCoroutine($coroutine, 'stop');
                                    $this->out("Coroutine#{$coroutine->getId()} continue to run...");
                                    $coroutine->resume();
                                    break;
                                default:
                                    assert(0 && 'never here');
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
                            $expression = implode(' ', $arguments);
                            if (!$expression) {
                                throw new DebuggerException('No expression');
                            }
                            $coroutine = $this->getCurrentCoroutine();
                            $index = $this->getCurrentFrameIndexExtended();
                            $result = var_dump_return($coroutine->eval($expression, $index));
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
                            $args = func_get_args();
                            Coroutine::run(function () use ($args) {
                                $this->reloading = true;
                                $this
                                    ->out('Program is running...')
                                    ->run(...$args);
                            });
                            goto _quit;
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

    /** @return $this */
    public static function runOnTTY(string $keyword = 'sdb')
    {
        return static::getInstance()->run($keyword);
    }

    public static function showExecutedSourceLines(bool $all = false): void
    {
        if ($all) {
            $handler = function () {
                static $lastFile;
                $call = debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS, 1)[0];
                ['file' => $file, 'line' => $line] = $call;
                if ($file === __FILE__) {
                    return;
                }
                if ($lastFile !== $file) {
                    $lastFile = $file;
                    echo '>>> ' . $file . PHP_EOL;
                }
                echo sprintf('%-6s%s' . PHP_EOL, $line, static::getFileLine($file, $line));
            };
        } else {
            $file = debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS, 1)[0]['file'] ?? '';
            $fp = @fopen($file, 'r');
            if ($fp === false) {
                throw IOException::getLast();
            }
            $lines = [];
            while (($line = fgets($fp)) !== false) {
                $lines[] = rtrim($line);
            }
            fclose($fp);
            $handler = function () use ($file, $lines) {
                $call = debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS, 1)[0];
                $callFile = $call['file'];
                $callLine = $call['line'];
                if ($callFile === $file) {
                    echo sprintf('%-6s%s' . PHP_EOL, $callLine, $lines[$callLine - 1]);
                }
            };
        }
        registerExtendedStatementHandler($handler);
    }

    protected static function getFileLine(string $file, int $lineNo): string
    {
        $fp = @fopen($file, 'r');
        if ($fp === false) {
            return "Unable to open {$file}";
        }
        $i = 0;
        while (($line = fgets($fp)) !== false) {
            if (++$i === $lineNo) {
                return rtrim($line);
            }
        }

        return "Unable to read {$file}:{$line}";
    }
}
