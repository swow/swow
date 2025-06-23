--TEST--
swow_siritz: wait2
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';

skip_if_not_zts();
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Siritz;

echo "Create siritz1\n";

$siritz = [];
for ($i = 0; $i < 10; $i++)
{
    $siritz[$i] = new Siritz(function () {
        require __DIR__ . '/../include/bootstrap.php';
        pseudo_random_sleep();
        echo "I'm in siritz1!\n";
    });
    $siritz[$i]->run();
}


for ($i = 0; $i < 10; $i++)
{
    $siritz[$i]->wait();
}

echo "Done1\n";

echo "Create siritz2\n";

$siritz = [];
for ($i = 0; $i < 10; $i++)
{
    $siritz[$i] = new Siritz(function () {
        require __DIR__ . '/../include/bootstrap.php';
        pseudo_random_sleep();
        echo "I'm in siritz2!\n";
    });
}

for ($i = 0; $i < 10; $i++)
{
    $siritz[$i]->run();
}

for ($i = 0; $i < 10; $i++)
{
    $siritz[$i]->wait();
}

echo "Done2\n";

echo "Create siritz3\n";

$siritz = [];
for ($i = 0; $i < 10; $i++)
{
    $siritz[$i] = new Siritz(function ($i) {
        require __DIR__ . '/../include/bootstrap.php';
        pseudo_random_sleep();
        echo "I'm in siritz3 $i!\n";
    }, $i);
    $siritz[$i]->run();
    $siritz[$i]->wait();
    echo "siritz3 end $i!\n";
}

echo "Done2\n";

?>
--EXPECT--
Create siritz1
I'm in siritz1!
I'm in siritz1!
I'm in siritz1!
I'm in siritz1!
I'm in siritz1!
I'm in siritz1!
I'm in siritz1!
I'm in siritz1!
I'm in siritz1!
I'm in siritz1!
Done1
Create siritz2
I'm in siritz2!
I'm in siritz2!
I'm in siritz2!
I'm in siritz2!
I'm in siritz2!
I'm in siritz2!
I'm in siritz2!
I'm in siritz2!
I'm in siritz2!
I'm in siritz2!
Done2
Create siritz3
I'm in siritz3 0!
siritz3 end 0!
I'm in siritz3 1!
siritz3 end 1!
I'm in siritz3 2!
siritz3 end 2!
I'm in siritz3 3!
siritz3 end 3!
I'm in siritz3 4!
siritz3 end 4!
I'm in siritz3 5!
siritz3 end 5!
I'm in siritz3 6!
siritz3 end 6!
I'm in siritz3 7!
siritz3 end 7!
I'm in siritz3 8!
siritz3 end 8!
I'm in siritz3 9!
siritz3 end 9!
Done2
