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

use Swow\Socket;
use Swow\Stream\EofStream;

use function extension_loaded;
use function is_array;
use function posix_isatty;

use const STDERR;
use const STDIN;
use const STDOUT;

class DebuggerIoStdIO implements DebuggerIoInterface
{
    protected Socket $input;
    protected Socket $output;
    protected Socket $error;
    protected bool $isTty = true;

    public function __construct(protected string $greeting)
    {
        if (extension_loaded('posix')) {
            $this->isTty =
                posix_isatty(STDIN) &&
                posix_isatty(STDOUT) &&
                posix_isatty(STDERR);
        }
        $this->input = (new EofStream("\n", type: Socket::TYPE_STDIN))->setReadTimeout(-1);
        $this->output = new EofStream("\n", Socket::TYPE_STDOUT);
        $this->error = new Socket(Socket::TYPE_STDERR);
        $this->out($this->greeting);
    }

    /** @return string[] */
    public function in(): string
    {
        $this->out("\r> ");
        return $this->input->recvMessageString();
    }

    public function out(string|array $data): static
    {
        if (is_array($data)) {
            $this->output->write($data);
        } else {
            $this->output->send($data);
        }
        return $this;
    }

    public function error(string|array $data): static
    {
        if (is_array($data)) {
            $this->error->write($data);
        } else {
            $this->error->send($data);
        }
        return $this;
    }

    public function isTty(): bool
    {
        return $this->isTty;
    }

    public function quit(): void
    {
        $this->out(":) It's StdIO mode, can't quit, if you want to terminate the program, please use `shutdown` command\n");
    }
}
