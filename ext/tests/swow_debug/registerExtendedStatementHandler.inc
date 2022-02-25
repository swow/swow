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

require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;
use function Swow\Debug\registerExtendedStatementHandler;

$lines = preg_split("/\r\n|\n/", file_get_contents(__FILE__));

define('BREAK_ME', implode(' ', ['break', 'me']));

// find out which line to break
foreach ($lines as $i => $line) {
    if (strpos($line, BREAK_ME) !== false) {
        $break_line = $i + 1 /* index starts from 0, lines start from 1*/;
        break;
    }
}
if (!isset($break_line)) {
    throw new Exception('Cannot find "' . BREAK_ME . '" mark in tests file');
}

function remote_callback()
{
    for ($i = 0; $i < 20; $i++) {
        // specified situation
        if ($i > 10) {
            $some_var = 42;
            // call back below will break here
            var_dump('break me');
            // then it modified $some_var and $i
            var_dump($some_var);
        }
    }
}

$remote_coro = new Coroutine(function () {
    // we do not dynamically call things to match PHP RFC
    // see https://wiki.php.net/rfc/forbid_dynamic_scope_introspection
    remote_callback();
});

try {
    registerExtendedStatementHandler(function () use ($break_line, $remote_coro) {
        // stack at this context if called:
        // coroutine closure above at break_me mark (level:2)
        // |_ this closure (level:1)
        //    |_ internal getExecutedLineno function (level:0)
        if ($remote_coro->getExecutedLineno(2) === $break_line) {
            // breaking;
            // get some remote info
            var_dump($remote_coro->getDefinedVars(2));
            // modify remote things
            $remote_coro->eval('$some_var=0;', 2);
            // skip last round
            $remote_coro->eval('$i+=10;', 2);
        }
    });
} catch (Error $e) {
    var_dump($e->getMessage());
    exit(1);
}

// run remote coro
$remote_coro->resume();

echo "Done\n";
