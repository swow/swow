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
    public function __toString()
    {
        $file = $this->getFile();
        $line = $this->getLine();
        $msg = $this->getMessage();
        $trace = $this->getTraceAsString();
        foreach ($this->getTrace() as $frame) {
            $file = $frame['file'] ?? 'Unknown';
            $line = $frame['line'] ?? 0;
            if ($file !== __FILE__) {
                break;
            }
        }

        return 'AssertException: ' . (empty($msg) ? '' : "{$msg} ") . "in {$file} on line {$line}\nStack trace: \n{$trace}";
    }
}
