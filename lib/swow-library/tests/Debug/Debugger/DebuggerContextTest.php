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

namespace Swow\Tests\Debug\Debugger;

use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\TestCase;
use Swow\Coroutine;
use Swow\Debug\Debugger\DebugContext;
use Swow\Debug\Debugger\Debugger;
use Swow\Debug\Debugger\DebuggerCommandContext;
use WeakReference;

use function count;

/** @internal */
#[CoversClass(Debugger::class)]
#[CoversClass(DebugContext::class)]
#[CoversClass(DebuggerCommandContext::class)]
final class DebuggerContextTest extends TestCase
{
    public function testCommandArgumentsAndReturnStateRemainIndependent(): void
    {
        $command = new DebuggerCommandContext('frame', ['2', 'source']);
        $other = new DebuggerCommandContext('next', []);
        $this->assertSame('frame', $command->getCommand());
        $this->assertSame(['2', 'source'], $command->getArguments());
        $this->assertSame('2', $command->getArgument(0));
        $this->assertSame('source', $command->getArgument(1));
        $this->assertNull($command->getArgument(2));
        $this->assertNull($command->getArgument(-1));
        $this->assertSame(DebuggerCommandContext::RETURN_TO_NONE, $command->getReturnTo());
        $command->setReturnTo(DebuggerCommandContext::RETURN_TO_NEXT);
        $this->assertSame(DebuggerCommandContext::RETURN_TO_NEXT, $command->getReturnTo());
        $this->assertSame(DebuggerCommandContext::RETURN_TO_NONE, $other->getReturnTo());
    }

    public function testDebugContextFollowsItsCoroutineWithoutRetainingIt(): void
    {
        $coroutine = Coroutine::run(static function (): void {
            Coroutine::yield();
        });
        try {
            $context = Debugger::getDebugContextOfCoroutine($coroutine);
            $current = Debugger::getDebugContextOfCoroutine(Coroutine::getCurrent());
            $this->assertNotSame($current, $context);
            $this->assertFalse($context->stop);
            $this->assertFalse($context->stopped);
            $context->stop = true;
            $this->assertSame($context, Debugger::getDebugContextOfCoroutine($coroutine));
            $this->assertTrue(Debugger::getDebugContextOfCoroutine($coroutine)->stop);
            $reference = WeakReference::create($coroutine);
            $coroutine->resume();
            unset($coroutine);
            gc_collect_cycles();
            $this->assertNull($reference->get());
        } finally {
            if (isset($coroutine) && $coroutine->isAlive()) {
                $coroutine->kill();
            }
        }
    }

    public function testSuspendedCoroutineInfoAndTraceSelection(): void
    {
        $coroutine = Coroutine::run(static function (): void {
            Coroutine::yield();
        });
        try {
            $trace = Debugger::getTraceOfCoroutine($coroutine);
            $this->assertNotEmpty($trace);
            $this->assertSame($trace[0], Debugger::getTraceOfCoroutine($coroutine, 0));
            $this->assertSame([], Debugger::getTraceOfCoroutine($coroutine, count($trace) + 1));
            $info = Debugger::getSimpleInfoOfCoroutine($coroutine, false);
            $this->assertSame($coroutine->getId(), $info['id']);
            $this->assertSame($coroutine->getStateName(), $info['state']);
            $this->assertSame($coroutine->getSwitches(), $info['switches']);
            $this->assertArrayNotHasKey('executing', $info);
            $full = Debugger::getSimpleInfoOfCoroutine($coroutine, true);
            $this->assertSame($info['id'], $full['id']);
            $this->assertIsString($full['executing']);
            $this->assertStringContainsString(basename(__FILE__), $full['source_position']);
        } finally {
            if ($coroutine->isAlive()) {
                $coroutine->resume();
            }
        }
    }
}
