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

use Swow\Channel;
use Swow\ChannelException;
use Swow\Coroutine;
use Swow\Errno;
use Swow\Signal;
use Swow\Utils\Handler;

use function basename;
use function end;
use function explode;
use function Swow\Debug\registerExtendedStatementHandler;

trait DebuggerBreakpointTrait
{
    protected ?Handler $breakPointHandler = null;

    /** @var string[] */
    protected array $breakPoints = [];

    public function __destructDebuggerBreakpoint(): void
    {
        if ($this->breakPointHandler) {
            $this->breakPointHandler->remove();
            $this->breakPointHandler = null;
        }
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
            $traceDiffLevel = static::getCoroutineTraceDiffLevel($coroutine, __FUNCTION__);
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
}
