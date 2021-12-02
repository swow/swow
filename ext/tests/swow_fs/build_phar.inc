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

const class1_php = <<<'PHP'
<?php
class Class1{
    public $myarg;
    function __construct($myarg){
        echo "Class1 initializing" . PHP_EOL;
        $this->myarg = $myarg;
    }
    function run(){
        usleep(1000);
        echo "Class1 run() is called" . PHP_EOL;
    }
}
PHP;

const class2_php = <<<'PHP'
<?php
require_once __DIR__ . "/cororun.php";
%s
class Class2{
    public $class1;
    function __construct($myarg){
        echo "Class2 initializing" . PHP_EOL;
    }
    function run(){
        usleep(1000);
        echo "Class2 run() is called" . PHP_EOL;
        cororun(function(){
            $this->class1 = new Class1("c1");
            usleep(1000);
        });
    }
}
PHP;

const autoload_php = <<<'PHP'
<?php
spl_autoload_register(static function ($class){
    require __DIR__ . "/" . $class . ".php";
});
PHP;

const cororun_php = <<<'PHP'
<?php
function cororun($x){
    if(extension_loaded("Swow")){
        usleep(1000);
        $wr = new \Swow\Sync\WaitReference();
        \Swow\Coroutine::run(function() use ($x, $wr) {
            $x();
        });
        \Swow\Sync\WaitReference::wait($wr);
    }else{
        $x();
    }
}
PHP;

const run_head = <<<'PHP'
<?php
require_once __DIR__ . "/cororun.php";
echo "on run.php" . PHP_EOL;
PHP;
const test_autoload_str = <<<'PHP'
// include with spl autoloader
require_once __DIR__ . "/autoload.php";
cororun(function(){
    $x = new Class1("a");
    $x->run();
});
cororun(function(){
    $x = new Class2("b");
    $x->run();
});
PHP;
const test_include_str = <<<'PHP'
// include with include_once
cororun(function(){
    include_once __DIR__ . "/Class1.php";
    $x = new Class1("a");
    $x->run();
});
cororun(function(){
    include_once __DIR__ . "/Class2.php";
    $x = new Class2("b");
    $x->run();
});
PHP;
const test_require_str = <<<'PHP'
// include with require_once
cororun(function(){
    require_once __DIR__ . "/Class1.php";
    $x = new Class1("a");
    $x->run();
});
cororun(function(){
    require_once __DIR__ . "/Class2.php";
    $x = new Class2("b");
    $x->run();
});
PHP;

const TEST_AUTOLOAD = 0;
const TEST_INCLUDE = 1;
const TEST_REQUIRE = 2;

function build_phar($pharFile = __DIR__ . '/phartest.phar', $test_type = TEST_AUTOLOAD)
{
    if (file_exists($pharFile)) {
        unlink($pharFile);
    }
    $phar = new \Phar($pharFile);
    switch ($test_type) {
        case TEST_AUTOLOAD:
            $phar->addFromString('autoload.php', autoload_php);
            $phar->addFromString('run.php', run_head . test_autoload_str);
            $phar->addFromString('Class2.php', sprintf(class2_php, 'require_once __DIR__ . "/autoload.php";'));
            break;
        case TEST_INCLUDE:
            $phar->addFromString('run.php', run_head . test_include_str);
            $phar->addFromString('Class2.php', sprintf(class2_php, 'include_once __DIR__ . "/Class1.php";'));
            break;
        case TEST_REQUIRE:
            $phar->addFromString('run.php', run_head . test_require_str);
            $phar->addFromString('Class2.php', sprintf(class2_php, 'require_once __DIR__ . "/Class1.php";'));
            break;
        default:
            throw new Exception('no such type');
    }
    $phar->addFromString('cororun.php', cororun_php);
    $phar->addFromString('Class1.php', class1_php);
    if (extension_loaded('zlib')) {
        $phar->compressFiles(\Phar::GZ);
    }
    $phar->stopBuffering();
    $phar->setStub($phar->createDefaultStub('run.php'));
}
