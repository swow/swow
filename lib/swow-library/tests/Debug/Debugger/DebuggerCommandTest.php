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
use PHPUnit\Framework\Attributes\DataProvider;
use PHPUnit\Framework\TestCase;
use Swow\Coroutine;
use Swow\Debug\Debugger\Debugger;
use Swow\Debug\Debugger\DebuggerCommandContext;
use Swow\Debug\Debugger\DebuggerException;
use Swow\Debug\Debugger\DebuggerIoInterface;
use WeakReference;

use function is_array;

/** @internal */
#[CoversClass(Debugger::class)]
#[CoversClass(DebuggerCommandContext::class)]
final class DebuggerCommandTest extends TestCase
{
    public function testCommandsUseTheReceivingDebugger(): void
    {
        $firstIo = new StreamDebuggerIo();
        $first = new CommandDebugger($firstIo);
        $first->dispatch('p', ['21 * 2']);
        $this->assertSame("int(42)\n", $firstIo->readOutput());
        $secondIo = new StreamDebuggerIo();
        $second = new CommandDebugger($secondIo);
        $second->dispatch('P', ['7']);
        $this->assertSame("int(7)\n", $secondIo->readOutput());
        $this->assertSame('', $firstIo->readOutput());
        $reference = WeakReference::create($first);
        unset($first);
        gc_collect_cycles();
        $this->assertNull($reference->get());
    }

    #[DataProvider('provideInvalidCommandsCases')]
    public function testInvalidCommands(string $command, array $arguments, string $message): void
    {
        $debugger = new CommandDebugger(new StreamDebuggerIo());
        $this->expectException(DebuggerException::class);
        $this->expectExceptionMessage($message);
        $debugger->dispatch($command, $arguments);
    }

    /** @return iterable<string, array{string, list<string>, string}> */
    public static function provideInvalidCommandsCases(): iterable
    {
        yield 'unknown name' => ['missing-command', [], "Unknown command 'missing-command'"];
        yield 'non-printable name' => ["\0", [], "Unknown command '00'"];
        yield 'invalid coroutine' => ['co', ['wrong'], 'Coroutine id must be numeric'];
        yield 'missing coroutine' => ['co', ['-1'], 'Not found'];
        yield 'invalid frame' => ['f', ['wrong'], 'Frame index must be numeric'];
        yield 'missing breakpoint' => ['b', [], 'Invalid break point'];
        yield 'invalid list count' => ['l', ['wrong'], 'line no must be numeric'];
        yield 'no selected source' => ['l', [], 'No source file was selected'];
        yield 'empty expression' => ['p', [], 'No expression'];
        yield 'no debugging session' => ['c', [], 'Not in debugging'];
        yield 'invalid zombie duration' => ['z', ['wrong'], 'Time must be numeric'];
        yield 'missing kill target' => ['kill', [], 'Required coroutine id'];
    }

    public function testCoroutineSelectionSourceMappingAndOwnedTermination(): void
    {
        $io = new StreamDebuggerIo();
        $debugger = new CommandDebugger($io);
        $coroutine = Coroutine::run(static function (): void {
            $token = 'swow-local';
            Coroutine::yield();
        });
        try {
            $debugger->setSourceMapHandler(static fn(string $position): string => str_replace(basename(__FILE__), 'mapped.php', $position));
            $debugger->dispatch('ps');
            $this->assertStringContainsString('mapped.php', $io->readOutput());
            $debugger->dispatch('co', [(string) $coroutine->getId()]);
            $this->assertSame($coroutine, $debugger->getCurrentCoroutine());
            $this->assertSame(0, $debugger->getCurrentFrameIndex());
            $this->assertNotNull($debugger->getCurrentSourceFile());
            $this->assertStringContainsString('Coroutine::yield()', $io->readOutput());
            $debugger->dispatch('p', ['$token']);
            $this->assertSame("string(10) \"swow-local\"\n", $io->readOutput());
            $debugger->dispatch('vars');
            $this->assertStringContainsString('swow-local', $io->readOutput());
            $debugger->dispatch('bt');
            $this->assertStringContainsString('mapped.php', $io->readOutput());
            $debugger->dispatch('f', ['0']);
            $this->assertStringContainsString('Coroutine::yield()', $io->readOutput());
            $debugger->dispatch('l', ['2']);
            $this->assertNotSame('', $io->readOutput());
            $debugger->dispatch('kill', [(string) $coroutine->getId()]);
            $this->assertFalse($coroutine->isAlive());
            $this->assertStringContainsString('killed', $io->readOutput());
            $debugger->dispatch('kill', ['-1']);
            $this->assertStringContainsString('not exists', $io->readOutput());
        } finally {
            if ($coroutine->isAlive()) {
                $coroutine->kill();
            }
        }
    }

    public function testCommandInputOutputAndHelpAliases(): void
    {
        $io = new StreamDebuggerIo("  p  return 42;  \n");
        $debugger = new CommandDebugger($io);
        $this->assertSame(['p', 'return', '42;'], $debugger->in());
        $this->assertSame($debugger, $debugger->setLastCommand(['p', 'return', '42;']));
        $this->assertSame(['p', 'return', '42;'], $debugger->getLastCommand());
        $debugger->dispatch('h');
        $help = $io->readOutput();
        $this->assertStringContainsString('backtrace', $help);
        $this->assertStringContainsString('bt', $help);
        $this->assertStringContainsString('continue', $help);
        $debugger->out(['first', 'second'])->error('failure')->cr()->lf()->clear();
        $this->assertSame("firstsecond\r\n", $io->readOutput());
        $this->assertSame('failure', $io->readError());
    }

    public function testExecutionUsesAnIndependentCoroutine(): void
    {
        $io = new StreamDebuggerIo();
        $debugger = new CommandDebugger($io);
        $current = Coroutine::getCurrent();
        $debugger->commandExec(new DebuggerCommandContext('exec', ['Swow\Coroutine::getCurrent()->getId()']));
        $output = $io->readOutput();
        $this->assertMatchesRegularExpression('/^int\([0-9]+\)\n$/', $output);
        $this->assertNotSame("int({$current->getId()})\n", $output);
        $this->assertSame($current, $debugger->getCurrentCoroutine());
    }

    public function testContinueResumesAnExplicitlyStoppedCoroutine(): void
    {
        $io = new StreamDebuggerIo();
        $debugger = new CommandDebugger($io);
        $completed = false;
        $coroutine = Coroutine::run(static function () use (&$completed): void {
            Debugger::break();
            $completed = true;
        });
        try {
            $context = Debugger::getDebugContextOfCoroutine($coroutine);
            $this->assertTrue($context->stopped);
            $this->assertFalse($completed);
            $this->assertSame('stopped', Debugger::getSimpleInfoOfCoroutine($coroutine, false)['state']);
            $debugger->dispatch('co', [(string) $coroutine->getId()]);
            $this->assertSame($coroutine, $debugger->getCurrentCoroutine());
            $this->assertNotSame('', $io->readOutput());
            $debugger->dispatch('c');
            $this->assertTrue($completed);
            $this->assertFalse($context->stopped);
            $this->assertFalse($context->stop);
            $this->assertFalse($coroutine->isAlive());
            $this->assertStringContainsString('continue to run', $io->readOutput());
        } finally {
            if ($coroutine->isAlive()) {
                $coroutine->kill();
            }
        }
    }

    public function testSourceSelectionDoesNotRetainAFileAfterAnInternalFrame(): void
    {
        $io = new StreamDebuggerIo();
        $debugger = new CommandDebugger($io);
        $debugger->showSourceFileContentByTrace([['file' => __FILE__, 'line' => 1]], 0);
        $this->assertSame(__FILE__, $debugger->getCurrentSourceFile()->getPathname());
        $this->assertSame(1, $debugger->getCurrentSourceFileLine());
        $this->assertStringContainsString('<?php', $io->readOutput());
        $debugger->showSourceFileContentByTrace([['function' => 'internal']], 0);
        $this->assertNull($debugger->getCurrentSourceFile());
        $this->assertSame(0, $debugger->getCurrentSourceFileLine());
        $this->assertSame("\n", $io->readOutput());
    }
}

/** @internal */
final class CommandDebugger extends Debugger
{
    public function __construct(DebuggerIoInterface $io)
    {
        parent::__construct($io);
    }

    /** @param list<string> $arguments */
    public function dispatch(string $command, array $arguments = []): void
    {
        $this->executeCommand($command, $arguments);
    }
}

/**
 * Real file streams isolate command I/O without replacing coroutine inspection.
 * @internal
 */
final class StreamDebuggerIo implements DebuggerIoInterface
{
    /** @var resource */
    private $input;

    /** @var resource */
    private $output;

    /** @var resource */
    private $error;

    public function __construct(string $input = '')
    {
        $this->input = fopen('php://temp', 'w+b');
        $this->output = fopen('php://temp', 'w+b');
        $this->error = fopen('php://temp', 'w+b');
        fwrite($this->input, $input);
        rewind($this->input);
    }

    public function __destruct()
    {
        fclose($this->input);
        fclose($this->output);
        fclose($this->error);
    }

    public function in(): string
    {
        return fgets($this->input) ?: '';
    }

    public function out(string|array $data): static
    {
        fwrite($this->output, is_array($data) ? implode('', $data) : $data);
        return $this;
    }

    public function error(string|array $data): static
    {
        fwrite($this->error, is_array($data) ? implode('', $data) : $data);
        return $this;
    }

    public function isTty(): bool
    {
        return false;
    }

    public function quit(): void
    {
        $this->out('Bye!');
    }

    public function readOutput(): string
    {
        return $this->read($this->output);
    }

    public function readError(): string
    {
        return $this->read($this->error);
    }

    /** @param resource $stream */
    private function read($stream): string
    {
        rewind($stream);
        $contents = stream_get_contents($stream);
        ftruncate($stream, 0);
        rewind($stream);
        return $contents;
    }
}
