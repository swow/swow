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

namespace Swow\Tests\Context;

use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\TestCase;
use Swow\Context\Context;
use Swow\Context\CoroutineContext;
use Swow\Coroutine;
use WeakReference;

/** @internal */
#[CoversClass(CoroutineContext::class)]
final class CoroutineContextTest extends TestCase
{
    public function testContextBelongsToCoroutine(): void
    {
        $first = new Coroutine(static function (): void {});
        $second = new Coroutine(static function (): void {});
        $context = CoroutineContext::get($first);
        $context['request_id'] = 'first';
        $this->assertInstanceOf(Context::class, $context);
        $this->assertSame($context, CoroutineContext::get($first));
        $this->assertNotSame($context, CoroutineContext::get($second));
        $this->assertFalse(CoroutineContext::get($second)->offsetExists('request_id'));
    }

    public function testCurrentCoroutineUsesItsExplicitContext(): void
    {
        $coroutine = new Coroutine(function (): void {
            $context = CoroutineContext::get();
            $context['request_id'] = 'inside';
            $this->assertSame($context, CoroutineContext::get(Coroutine::getCurrent()));
            Coroutine::yield();
            $this->assertSame('outside', CoroutineContext::get()['request_id']);
        });
        try {
            $coroutine->resume();
            $this->assertSame('inside', CoroutineContext::get($coroutine)['request_id']);
            CoroutineContext::get($coroutine)['request_id'] = 'outside';
            $coroutine->resume();
        } finally {
            if ($coroutine->isAlive()) {
                $coroutine->kill();
            }
        }
    }

    public function testContextMapDoesNotRetainCompletedCoroutine(): void
    {
        $coroutine = Coroutine::run(static function (): void {
            CoroutineContext::get()['request_id'] = 'released';
        });
        $coroutineReference = WeakReference::create($coroutine);
        $contextReference = WeakReference::create(CoroutineContext::get($coroutine));
        unset($coroutine);
        gc_collect_cycles();
        $this->assertNull($coroutineReference->get());
        $this->assertNull($contextReference->get());
    }
}
