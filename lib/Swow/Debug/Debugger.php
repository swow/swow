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

use SplFileObject;
use Swow\Coroutine;
use Swow\Signal;
use Swow\Socket;
use Throwable;
use function Swow\Util\var_dump_return;
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

    /* @var Debugger[] */
    private static $debuggerMap = [];

    /* @var $this */
    protected static $instance;

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

    /* @var array */
    protected $breakPoints = [];

    public static function getInstance(): self
    {
        return static::$instance ?? (static::$instance = new static());
    }

    public function __construct()
    {
        $this->input = new Socket(Socket::TYPE_STDIN);
        $this->output = new Socket(Socket::TYPE_STDOUT);
        $this->error = new Socket(Socket::TYPE_STDERR);
        $this
            ->setCurrentCoroutine(Coroutine::getCurrent())
            ->checkPathMap();
        self::$debuggerMap[spl_object_id($this)] = $this;
    }

    public function __destruct()
    {
        unset(self::$debuggerMap[spl_object_id($this)]);
        $breakPointHandlerShouldBeRemoved = true;
        foreach (self::$debuggerMap as $debugger) {
            if ($debugger->breakPointHandlerEnabled) {
                $breakPointHandlerShouldBeRemoved = false;
                break;
            }
        }
        if ($breakPointHandlerShouldBeRemoved) {
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

    public function out(string $string = '', bool $newline = true): self
    {
        $this->output->write([$string, $newline ? "\n" : null]);

        return $this;
    }

    public function exception(string $string = '', bool $newline = true): self
    {
        $this->output->write([$string, $newline ? "\n" : null]);

        return $this;
    }

    public function error(string $string = '', bool $newline = true): self
    {
        $this->error->write([$string, $newline ? "\n" : null]);

        return $this;
    }

    public function cr(): self
    {
        return $this->out("\r", false);
    }

    public function lf(): self
    {
        return $this->out("\n", false);
    }

    public function clear(): self
    {
        $this->output->sendString("\033c");

        return $this;
    }

    public function table(array $table, bool $newline = true): self
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
        if ($sourcePositionHandler != null) {
            return $sourcePositionHandler($sourcePosition);
        }

        return $sourcePosition;
    }

    public function setLastCommand(string $lastCommand): self
    {
        $this->lastCommand = $lastCommand;

        return $this;
    }

    public function setSourcePositionHandler(callable $sourcePositionHandler): self
    {
        $this->sourcePositionHandler = $sourcePositionHandler;

        return $this;
    }

    public function checkPathMap(): self
    {
        $pathMap = getenv('SDB_PATH_MAP');
        if (is_string($pathMap) && file_exists($pathMap)) {
            $pathMap = json_decode(file_get_contents($pathMap), true);
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

    protected function setCursorVisibility(bool $bool): self
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

    protected function setCurrentCoroutine(Coroutine $coroutine): self
    {
        $this->currentCoroutine = $coroutine;
        $this->currentFrameIndex = 0;

        return $this;
    }

    public function getCurrentFrameIndex(): int
    {
        return $this->currentFrameIndex;
    }

    public function getCurrentFrameIndexExtended(): int
    {
        return $this->currentFrameIndex + static::getExtendedLevelOfCoroutine($this->getCurrentCoroutine());
    }

    protected function setCurrentFrameIndex(int $index): self
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

    public function setCurrentSourceFile(SplFileObject $currentSourceFile): self
    {
        $this->currentSourceFile = $currentSourceFile;

        return $this;
    }

    public function getCurrentSourceFileLine(): int
    {
        return $this->currentSourceFileLine;
    }

    public function setCurrentSourceFileLine(int $currentSourceFileLine): self
    {
        $this->currentSourceFileLine = $currentSourceFileLine;

        return $this;
    }

    protected static function getMaxLengthOfStringLine(string $string): int
    {
        $maxLength = 0;
        $lines = explode("\n", $string);
        foreach ($lines as $line) {
            $maxLength = max($maxLength, strlen($line));
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
        switch (1) {
            case is_int($value):
            case is_float($value):
                return "{$value}";
            case is_null($value):
                return 'null';
            case is_bool($value):
                return $value ? 'true' : 'false';
            case is_string($value):
            {
                $value = (ctype_print($value) ? $value : bin2hex($value));
                $maxLength = $forArgs ? 8 : 512;
                if (strlen($value) > $maxLength) {
                    $value = substr($value, 0, $maxLength) . '...';
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
            if ($args) {
                $argsString = '';
                foreach ($args as $argName => $argValue) {
                    $argsString .= static::convertValueToString($argValue) . ', ';
                }
                $argsString = rtrim($argsString, ', ');
            } else {
                $argsString = '';
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
            if ($frameIndex !== null) {
                if ($index !== $frameIndex) {
                    continue;
                }
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

    protected function showTrace(array $trace, ?int $frameIndex = null, bool $newLine = true): self
    {
        $traceTable = $this::convertTraceToTable($trace, $frameIndex);
        foreach ($traceTable as &$traceItem) {
            $traceItem['source position'] = $this->callSourcePositionHandler($traceItem['source position']);
        }
        unset($traceItem);
        $this->table($traceTable, $newLine);

        return $this;
    }

    protected static function getStateNameOfCoroutine(Coroutine $coroutine)
    {
        if ($coroutine->__stopped ?? false) {
            $state = 'stopped';
        } else {
            $state = $coroutine->getStateName();
        }

        return $state;
    }

    protected static function getExtendedLevelOfCoroutine(Coroutine $coroutine): int
    {
        if ($coroutine->__stopped ?? false) {
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

    protected function showCoroutine(Coroutine $coroutine, bool $newLine = true): self
    {
        $debugInfo = static::getSimpleInfoOfCoroutine($coroutine, false);
        $trace = $this::getTraceOfCoroutine($coroutine);
        $this->table([$debugInfo], !$trace);
        if ($trace) {
            $this->cr()->showTrace($trace, null, $newLine);
        }

        return $this;
    }

    /** @param Coroutine[] $coroutines */
    public function showCoroutines(array $coroutines): self
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

    protected static function getSourceFileContentAsTable(string $file, int $line, SplFileObject &$sourceFile = null, int $lineCount = self::SOURCE_FILE_DEFAULT_LINE_COUNT): array
    {
        $sourceFile = null;
        if ($line < 2) {
            $startLine = $line;
        } else {
            $startLine = $line - ($lineCount - static::SOURCE_FILE_CONTENT_PADDING - 1);
        }
        $file = new SplFileObject($file);
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
        $sourceFile = null;
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

    public function showSourceFileContentByTrace(array $trace, int $frameIndex, bool $following = false): self
    {
        $file = $this->getCurrentSourceFile();
        $line = $this->getCurrentSourceFileLine();
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
            $this->lf();

            return $this;
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

    protected function showFollowingSourceFileContent(int $lineCount = self::SOURCE_FILE_DEFAULT_LINE_COUNT): self
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

    public function addBreakPoint(string $point): self
    {
        $this
            ->checkBreakPointHandler()
            ->breakPoints[] = $point;

        return $this;
    }

    public static function break(): void
    {
        $coroutine = Coroutine::getCurrent();
        $coroutine->__stopped = true;
        Coroutine::yield();
        if ($coroutine->__stop ?? false) {
            $coroutine->__stopped = false;
        } else {
            unset($coroutine->__stopped);
        }
    }

    public static function breakOn(string $point): self
    {
        return static::getInstance()->addBreakPoint($point);
    }

    protected static function breakPointHandler(): void
    {
        $coroutine = Coroutine::getCurrent();
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
        foreach (self::$debuggerMap as $debugger) {
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
                    break 2;
                }
            }
        }
        if ($hit) {
            $coroutine->__stop = true;
        }
        if ($coroutine->__stop ?? false) {
            static::break();
        }
    }

    private function checkBreakPointHandler(): self
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
        if (!($coroutine->__stopped ?? false)) {
            $this->out('Waiting...');
        }
        while (!($coroutine->__stopped ?? false)) {
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
            if ($coroutine->getState() === $coroutine::STATE_LOCKED) {
                continue;
            }

            return false;
        }

        return true;
    }

    public function logo(): self
    {
        return $this->clear()->out($this::SDB)->lf();
    }

    public function run(string $keyword = ''): self
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
        if (strlen($keyword) > 0) {
            $this->lf()->out("You can input '{$keyword}' to to call out the debug interface...");
        }
        _restart:
        if (strlen($keyword) > 0) {
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
                    $arguments = array_filter($arguments, function (string $value) {
                        return strlen($value) !== 0;
                    });
                    $command = array_shift($arguments);
                    switch ($command) {
                        case 'ps':
                            $this->showCoroutines(Coroutine::getAll());
                            break;
                        case 'attach':
                        case 'co':
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
                                $coroutine->__stop = true;
                            }
                            $this->setCurrentCoroutine($coroutine);
                            $in = 'bt';
                            goto _next;
                        case 'bt':
                            $this->showCoroutine($this->getCurrentCoroutine(), false)
                                ->showSourceFileContentByTrace($this->getCurrentCoroutineTrace(), 0, true);
                            break;
                        case 'f':
                            $frameIndex = $arguments[0] ?? null;
                            if ($frameIndex === null || !is_numeric($frameIndex)) {
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
                            $breakPoint = $arguments[0] ?? '';
                            if (strlen($breakPoint) === 0) {
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
                        case 'c':
                            $coroutine = $this->getCurrentCoroutine();
                            if (!($coroutine->__stopped ?? false)) {
                                if ($coroutine->__stop ?? false) {
                                    $this->waitStoppedCoroutine($coroutine);
                                }
                                throw new DebuggerException('Not in debugging');
                            }
                            switch ($command) {
                                case 'n':
                                    $coroutine->resume();
                                    $this->waitStoppedCoroutine($coroutine);
                                    $in = 'f 0';
                                    goto _next;
                                case 'c':
                                    unset($coroutine->__stop);
                                    $this->out("Coroutine#{$coroutine->getId()} continue to run...");
                                    $coroutine->resume();
                                    break;
                                default:
                                    assert(0 && 'never here');
                            }
                            break;
                        case 'l':
                            $lineCount = $arguments[0] ?? null;
                            if ($lineCount === null || !is_numeric($lineCount)) {
                                $this->showFollowingSourceFileContent();
                            } else {
                                $this->showFollowingSourceFileContent((int) $lineCount);
                            }
                            break;
                        case 'p':
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
                            $tryAgain = false;
                            do {
                                foreach (Coroutine::getAll() as $coroutine) {
                                    if ($coroutine === Coroutine::getCurrent()) {
                                        continue;
                                    }
                                    if ($coroutine->getState() === $coroutine::STATE_LOCKED) {
                                        continue;
                                    }
                                    $this->out("Kill {$coroutine->getId()}...");
                                    $coroutine->kill();
                                    if ($coroutine->isAvailable()) {
                                        $this->out('Not fully killed, try again later...');
                                        $tryAgain = true;
                                    } else {
                                        $this->out('Killed');
                                    }
                                }
                            } while ($tryAgain);
                            $this->out('All coroutines has been killed');
                            break;
                        case 'clear':
                            $this->clear();
                            break;
                        case 'q':
                            $this->clear();
                            if (strlen($keyword) !== 0 && !static::isAlone()) {
                                /* we can input keyword to call out the debugger */
                                goto _restart;
                            }
                            goto _quit;
                        case 'r':
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

    public static function runOnTTY(string $keyword = 'sdb'): self
    {
        return static::getInstance()->run($keyword);
    }
}
