--TEST--
swow_coroutine: getExecutedFunctionName getExecutedFilename getExecutedLineno
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Coroutine;

function aStrange_function(bool $yield): void
{
    $coro = Coroutine::getCurrent();
    Assert::same($coro->getExecutedFunctionName(1), __FUNCTION__);
    Assert::same($coro->getExecutedFilename(1), __FILE__);
    Assert::same($coro->getExecutedLineno(1), __LINE__);
    if ($yield) {
        Coroutine::yield([__LINE__, __FILE__, __FUNCTION__]);
    }
}

class SomeStrangeClass
{
    public static function SfUNC(bool $yield): void
    {
        $coro = Coroutine::getCurrent();
        Assert::same($coro->getExecutedFunctionName(1), __CLASS__ . '::' . __FUNCTION__);
        Assert::same($coro->getExecutedFilename(1), __FILE__);
        Assert::same($coro->getExecutedLineno(1), __LINE__);
        if ($yield) {
            Coroutine::yield([__LINE__, __FILE__, __CLASS__ . '::' . __FUNCTION__]);
        }
    }

    public function PfUNC(bool $yield): void
    {
        $coro = Coroutine::getCurrent();
        Assert::same($coro->getExecutedFunctionName(1), __CLASS__ . '::' . __FUNCTION__);
        Assert::same($coro->getExecutedFilename(1), __FILE__);
        Assert::same($coro->getExecutedLineno(1), __LINE__);
        if ($yield) {
            Coroutine::yield([__LINE__, __FILE__, __CLASS__ . '::' . __FUNCTION__]);
        }
    }
}

$closure = function (...$_) {
    $coro = Coroutine::getCurrent();
    Assert::same($coro->getExecutedFunctionName(1), '{closure}');
    Assert::same($coro->getExecutedFilename(1), __FILE__);
    Assert::same($coro->getExecutedLineno(1), __LINE__);
};

$closure_yield = function (bool $yield) {
    $coro = Coroutine::getCurrent();
    Assert::same($coro->getExecutedFunctionName(1), '{closure}');
    Assert::same($coro->getExecutedFilename(1), __FILE__);
    Assert::same($coro->getExecutedLineno(1), __LINE__);
    if ($yield) {
        Coroutine::yield([__LINE__, __FILE__, __FUNCTION__]);
    }
};

$someStrangeObject = new SomeStrangeClass();

foreach ([
    $closure_yield,
    'aStrange_function',
    ['SomeStrangeClass', 'SfUNC'],
    [$someStrangeObject, 'PfUNC'],
] as $callable) {
    $callable(false);
    $remote_coro = new Coroutine(function () use ($callable) {
        $callable(true);
    });
    [$line, $file, $function] = $remote_coro->resume();
    try {
        Assert::same($remote_coro->getExecutedFunctionName(1), $function);
        Assert::same($remote_coro->getExecutedFilename(1), $file);
        Assert::same($remote_coro->getExecutedLineno(1), $line);
    } finally {
        $remote_coro->resume();
    }
}

register_shutdown_function($closure);
register_tick_function($closure);
set_error_handler($closure);

echo 'Done' . PHP_LF;
?>
--EXPECT--
Done
