--TEST--
swow_time: bad arguments passed in
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

set_error_handler(function(...$_){
    echo "caught a PHP7 warning\n";
});

foreach ([
    "sleep",
    "usleep",
    "msleep",
] as $func){
    try{
        echo "$func:";
        $func(-1);
    }catch(ValueError $e){
        echo "caught a PHP8 ValueError\n";
    }
}

try{
    echo "time_nanosleep:";
    time_nanosleep(-1, -2);
}catch(ValueError $e){
    echo "caught a PHP8 ValueError\n";
}

try{
    echo "time_sleep_until:";
    time_sleep_until(0);
}catch(ValueError $e){
    echo "caught a PHP8 ValueError\n";
}

echo "done";
?>
--EXPECTREGEX--
sleep:caught a (PHP8 ValueError|PHP7 warning)
usleep:caught a \1
msleep:caught a \1
time_nanosleep:caught a \1
time_sleep_until:caught a (PHP8 ValueError|PHP7 warning)
done
