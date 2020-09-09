<?php
/**
 * This file is part of Swow
 *
 * @link     https://github.com/swow/swow
 * @contact  twosee <twose@qq.com>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

namespace Swow;

class AssertException extends Exception
{
    protected $sourcePositionHook;

    public function setSourcePositionHook(callable $closure): self
    {
        $this->sourcePositionHook = $closure;

        return $this;
    }

    public function __toString()
    {
        $message = $this->getMessage();
        $file = $this->getFile();
        $line = $this->getLine();

        $hook = $this->sourcePositionHook;
        if ($hook !== null) {
            $hook($this, $file, $line);
        }

        return 'AssertException: ' .
            "{$message}" . ($message ? ' ' : '') .
            "in {$file} on line {$line}\n" .
            "Stack trace: \n{$this->getTraceAsString()}";
    }
}
