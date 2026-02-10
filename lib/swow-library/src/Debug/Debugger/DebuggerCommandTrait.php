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
/**
 * Author: Twosee <twose@qq.com>
 * Date: 2024/3/23 15:09
 */

namespace Swow\Debug\Debugger;

use Error;
use Swow\Channel;
use Swow\Coroutine;
use WeakMap;

use function array_keys;
use function bin2hex;
use function count;
use function implode;
use function is_numeric;
use function strlen;
use function strtolower;
use function substr;
use function Swow\Debug\var_dump_return;
use function usleep;
use function var_dump;

use const PHP_INT_MAX;

trait DebuggerCommandTrait
{
    protected static array $commandMethods = [];

    protected static array $commandShortNameMap = [
        'co' => 'coroutine',
        'bt' => 'backtrace',
        'f' => 'frame',
        'b' => 'breakpoint',
        'n' => 'next',
        's' => 'step',
        'step_in' => 'step',
        'c' => 'continue',
        'l' => 'list',
        'p' => 'print',
        'exec' => 'print',
        'z' => 'zombie',
        'q' => 'quit',
        'exit' => 'quit',
        'r' => 'run',
        'h' => 'help',
    ];

    protected static function convertCommandShortNameToFullName(string $shortName): string
    {
        return static::$commandShortNameMap[$shortName] ?? $shortName;
    }

    /** @param string[] $arguments */
    protected function executeCommand(string $command, array $arguments): void
    {
        $command = strtolower($command);
        $command = $this::convertCommandShortNameToFullName($command);
        if (!static::$commandMethods) {
            $reflectionCommandMethods = $this->reflection->getMethods();
            foreach ($reflectionCommandMethods as $method) {
                $methodName = $method->getName();
                if (str_starts_with($methodName, 'command')) {
                    $commandName = substr($methodName, strlen('command'));
                    $commandName = strtolower($commandName);
                    static::$commandMethods[$commandName] = $method->getClosure($this);
                }
            }
        }
        $method = static::$commandMethods[$command] ?? null;
        if (!$method) {
            if (!ctype_print($command)) {
                $command = bin2hex($command);
            }
            throw new DebuggerException("Unknown command '{$command}'");
        }
        $method(new DebuggerCommandContext($command, $arguments));
    }

    public function commandPs(): void
    {
        $this->showCoroutines(Coroutine::getAll());
    }

    public function commandBacktrace(): void
    {
        $this->showCoroutine($this->getCurrentCoroutine())
            ->showSourceFileContentByTrace($this->getCurrentCoroutineTrace(), 0, true);
    }

    public function commandCoroutine(DebuggerCommandContext $context): void
    {
        $this->_commandCoroutineOrAttach($context);
    }

    public function commandAttach(DebuggerCommandContext $context): void
    {
        $this->_commandCoroutineOrAttach($context);
    }

    public function _commandCoroutineOrAttach(DebuggerCommandContext $context): void
    {
        $id = $context->getArgument(0) ?? 'unknown';
        if (!is_numeric($id)) {
            throw new DebuggerException('Argument[1]: Coroutine id must be numeric');
        }
        $coroutine = Coroutine::get((int) $id);
        if (!$coroutine) {
            throw new DebuggerException("Coroutine#{$id} Not found");
        }
        if ($context->getCommand() === 'attach') {
            $this->checkBreakPointHandler();
            if ($coroutine === Coroutine::getCurrent()) {
                throw new DebuggerException('Attach debugger is not allowed');
            }
            static::getDebugContextOfCoroutine($coroutine)->stop = true;
        }
        $this->setCurrentCoroutine($coroutine);
        $this->commandBacktrace();
    }

    public function commandFrame(DebuggerCommandContext $context): void
    {
        $frameIndex = $context->getArgument(0);
        if (!is_numeric($frameIndex)) {
            throw new DebuggerException('Frame index must be numeric');
        }
        $frameIndex = (int) $frameIndex;
        if ($this->getCurrentFrameIndex() !== $frameIndex) {
            $this->out("Switch to frame {$frameIndex}\n");
        }
        $this->setCurrentFrameIndex($frameIndex);
        $trace = $this->getCurrentCoroutineTrace();
        $frameIndex = $this->getCurrentFrameIndex();
        $this
            ->showTrace($trace, $frameIndex)
            ->showSourceFileContentByTrace($trace, $frameIndex, true);
    }

    public function commandBreakPoint(DebuggerCommandContext $context): void
    {
        $breakPoint = $context->getArgument(0) ?? '';
        if ($breakPoint === '') {
            throw new DebuggerException('Invalid break point');
        }
        $coroutine = $this->getCurrentCoroutine();
        if ($coroutine === Coroutine::getCurrent()) {
            $this
                ->out("Added global break-point <{$breakPoint}>\n")
                ->addBreakPoint($breakPoint);
        }
    }

    public function commandNext(DebuggerCommandContext $context): void
    {
        $this->_commandDebugging($context);
    }

    public function commandStep(DebuggerCommandContext $context): void
    {
        $this->_commandDebugging($context);
    }

    public function commandContinue(DebuggerCommandContext $context): void
    {
        $this->_commandDebugging($context);
    }

    public function _commandDebugging(DebuggerCommandContext $context): void
    {
        $command = $context->getCommand();
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
            case 'next':
            case 'step':
                if ($command === 'n' || $command === 'next') {
                    $this->lastTraceDepth = $coroutine->getTraceDepth() - static::getCoroutineTraceDiffLevel($coroutine, 'nextCommand');
                }
                $coroutine->resume();
                $this->waitStoppedCoroutine($coroutine);
                $this->lastTraceDepth = PHP_INT_MAX;
                static $frame0Command = null;
                $this->commandFrame($frame0Command ??= new DebuggerCommandContext('frame', ['0']));
                break;
            case 'continue':
                static::getDebugContextOfCoroutine($coroutine)->stop = false;
                $this->out("Coroutine#{$coroutine->getId()} continue to run...\n");
                $coroutine->resume();
                break;
            default:
                throw new Error('Never here');
        }
    }

    public function commandList(DebuggerCommandContext $context): void
    {
        $lineCount = $context->getArgument(0);
        if ($lineCount === null) {
            $this->showFollowingSourceFileContent();
        } elseif (is_numeric($lineCount)) {
            $this->showFollowingSourceFileContent((int) $lineCount);
        } else {
            throw new DebuggerException('Argument[1]: line no must be numeric');
        }
    }

    public function commandPrint(DebuggerCommandContext $context): void
    {
        $expression = implode(' ', $context->getArguments());
        if (!$expression) {
            throw new DebuggerException('No expression');
        }
        $coroutine = $this->getCurrentCoroutine();
        $index = $this->getCurrentFrameIndexExtendedForExecution();
        $result = var_dump_return($coroutine->eval($expression, $index));
        $this->out($result);
    }

    public function commandExec(DebuggerCommandContext $context): void
    {
        $expression = implode(' ', $context->getArguments());
        if (!$expression) {
            throw new DebuggerException('No expression');
        }
        $transfer = new Channel();
        Coroutine::run(static function () use ($expression, $transfer): void {
            $transfer->push(Coroutine::getCurrent()->eval($expression));
        });
        // TODO: support ctrl + c (also support ctrl + c twice confirm on global scope?)
        $result = var_dump_return($transfer->pop());
        $this->out($result);
    }

    public function commandVars(DebuggerCommandContext $context): void
    {
        $coroutine = $this->getCurrentCoroutine();
        $index = $this->getCurrentFrameIndexExtendedForExecution();
        /** When using breakpoint, real backtrace is like this:
         * ```
         *     #0 [internal function]: Swow\Coroutine->__debugInfo()
         *     #1 /path/to/swow/lib/swow-library/src/Debug/Debugger/DebuggerBreakpointTrait.php(%d): var_dump(Array)
         *     #2 /path/to/swow/examples/debug/debugger/demo.php(%d): Swow\Debug\Debugger\Debugger::breakPointHandler()
         *     #3 /path/to/swow/examples/debug/debugger/demo.php(%d): {closur%s}()
         *     #4 [internal function]: {closur%s}()
         *     #5 {main}
         * ```
         * We can see that when we call breakPointHandler,
         * it's in a new function stack (see debug_call_extended_statement_handlers in C file),
         * then we can not access vars in the original function stack anymore,
         * I don't know why it's designed like this, but I have no time to fix it now,
         * but we still can use eval to access vars in the original function stack.
         */
        // $result = var_dump_return($coroutine->getDefinedVars($index));
        $result = var_dump_return($coroutine->eval('get_defined_vars()', $index));
        $this->out($result);
    }

    public function commandZombie(DebuggerCommandContext $context): void
    {
        $time = $context->getArgument(0);
        if (!is_numeric($time)) {
            throw new DebuggerException('Argument[1]: Time must be numeric');
        }
        $this->out("Scanning zombie coroutines ({$time}s)...\n");
        /** @var WeakMap<Coroutine, int> $switchesMap */
        $switchesMap = new WeakMap();
        foreach (Coroutine::getAll() as $coroutine) {
            $switchesMap[$coroutine] = $coroutine->getSwitches();
        }
        usleep((int) ($time * 1000 * 1000));
        $zombies = [];
        foreach ($switchesMap as $coroutine => $switches) {
            if ($coroutine->getSwitches() === $switches) {
                $zombies[] = $coroutine;
            }
        }
        $this
            ->out("Following coroutine maybe zombies:\n")
            ->showCoroutines($zombies);
    }

    public function commandKill(DebuggerCommandContext $context): void
    {
        $arguments = $context->getArguments();
        if (count($arguments) === 0) {
            throw new DebuggerException('Required coroutine id');
        }
        foreach ($arguments as $index => $argument) {
            if (!is_numeric($argument)) {
                $this->exception("Argument[{$index}] '{$argument}' is not numeric\n");
            }
        }
        foreach ($arguments as $argument) {
            $coroutine = Coroutine::get((int) $argument);
            if ($coroutine) {
                $coroutine->kill();
                $this->out("Coroutine#{$argument} killed\n");
            } else {
                $this->exception("Coroutine#{$argument} not exists\n");
            }
        }
    }

    public function commandKillAll(DebuggerCommandContext $context): void
    {
        Coroutine::killAll();
        $this->out("All coroutines has been killed\n");
    }

    public function commandClear(DebuggerCommandContext $context): void
    {
        $this->clear();
    }

    public function commandQuit(DebuggerCommandContext $context): void
    {
        $this->quit();
    }

    public function commandShutdown(DebuggerCommandContext $context): void
    {
        Coroutine::killAll();
        exit;
    }

    public function commandHelp(DebuggerCommandContext $context): void
    {
        $commandInfo = [];
        foreach (static::$commandMethods as $commandName => $_) {
            $commandShortNames = implode(', ', array_keys(static::$commandShortNameMap, $commandName, true));
            $commandInfo[] = ['command' => $commandName, 'alias' => $commandShortNames];
        }
        $this->table($commandInfo)->lf();
    }
}
