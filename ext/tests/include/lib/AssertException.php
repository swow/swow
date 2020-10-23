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

use function Swow\Debug\buildTraceAsString;

class AssertException extends Exception
{
    protected $traceToStringHandler;

    public function setTraceToStringHandler(callable $handler): self
    {
        $this->traceToStringHandler = $handler;

        return $this;
    }

    public function __toString()
    {
        $message = $this->getMessage();
        $trace = $this->getTrace();

        $handler = $this->traceToStringHandler;
        if ($handler !== null) {
            $trace = $handler($trace);
        }
        $file = $trace[0]['file'] ?? 'Unknown';
        $line = $trace[0]['line'] ?? 0;
        $traceString = buildTraceAsString($trace);

        return 'AssertException: ' .
            "{$message}" . ($message ? ' ' : '') .
            "in {$file} on line {$line}\n" .
            "Stack trace: \n{$traceString}";
    }
}
