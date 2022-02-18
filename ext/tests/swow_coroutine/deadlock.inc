<?php

fwrite(STDOUT, getmypid() . PHP_EOL);
Swow\Coroutine::yield();
fwrite(STDERR, 'Never here' . PHP_EOL);
