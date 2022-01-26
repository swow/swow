--TEST--
swow_fs: flock basic functionality
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip('close(2) syscall behaves strange on macOS', PHP_OS_FAMILY === 'Darwin');
skip_if_cannot_make_subprocess();
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Socket;
use Swow\Sync\WaitReference;

const TEST_THREADS = 24;
const TEST_LOCKNAME = __DIR__ . DIRECTORY_SEPARATOR . 'lockfile.txt';

// create pipe
if (PHP_OS_FAMILY !== 'Windows') {
    define('SOCK_NAME', '/tmp/swow_flock_' . getRandomBytes(8) . '.sock');
} else {
    define('SOCK_NAME', '\\\\?\\pipe\\swow_flock_' . getRandomBytes(8));
}

$sock = new Socket(Socket::TYPE_PIPE);
$sock->bind(SOCK_NAME)->listen();

// create lock file
@unlink(TEST_LOCKNAME);
$fd = fopen(TEST_LOCKNAME, 'w+');

// create child
$ext_enable = ' ';
$loaded = shell_exec(PHP_BINARY . ' -m');
if (strpos($loaded, 'Swow') === false) {
    $ext_enable = ' -dextension=swow ';
}
$p = popen(PHP_BINARY . $ext_enable . __DIR__ . DIRECTORY_SEPARATOR . 'flockchild.inc ' . SOCK_NAME, 'w');
if (!$p) {
    fprintf(STDERR, "failed create child\n");
    exit(1);
}

// wait for child connect
//printf("wait connecting in\n");
$conn = $sock->accept(10000);
//printf("accepted\n");

function tellchild($char)
{
    global $conn;
    $conn->sendString($char);
}
function waitchild($char)
{
    global $conn;
    $red = $conn->readString(1);
    var_dump($char);
    if (strncmp($char, $red, 1) != 0) {
        throw new Exception('child failed continue');
    }
}

// tell child lock file, wait for it's done
tellchild('1');
waitchild('a');

$wr = new WaitReference();

$badresult = false;
$coros = [];
// lock nb tests: should fail
for ($i = 0; $i < TEST_THREADS; $i++) {
    $coros[$i] = Swow\Coroutine::run(function () use (&$badresult, $wr, $fd) {
        $ret = flock($fd, LOCK_EX | LOCK_NB);
        if ($ret) {
            fprintf(STDERR, "LOCK_EX success, but it should not\n");
            $badresult = true;

            return;
        }
    });
}

// wait for all nb requests done
try {
    WaitReference::wait($wr, 5000);
} catch (Exception $e) {
    tellchild('e');
    throw $e;
}

if ($badresult) {
    tellchild('e');
    exit(1);
}

$count = 0;
$wr = new WaitReference();
// make stuck threads
for ($i = 0; $i < TEST_THREADS; $i++) {
    $coros[$i] = Swow\Coroutine::run(function () use (&$count, $wr, $fd) {
        $ret = flock($fd, LOCK_EX); // always stuck here
        flock($fd, LOCK_UN);
        $count++;
    });
}

$wr2 = new WaitReference();
// test any fs coro
$coros[$i] = Swow\Coroutine::run(function () use (&$badresult, $wr2) {
    stat(TEST_LOCKNAME);
});

// wait for ^ done
try {
    WaitReference::wait($wr2, 1000);
} catch (Exception $e) {
    tellchild('e');
    throw $e;
}

// tell child release lock
tellchild('2');
waitchild('b');

// wait for stucking coros done
try {
    WaitReference::wait($wr, 5000);
} catch (Exception $e) {
    throw $e;
}

if ($count != TEST_THREADS) {
    fprintf(STDERR, "not all coro done, that's impossible\n");
}

echo 'Done' . PHP_LF;
?>
--CLEAN--
<?php
unlink(__DIR__ . DIRECTORY_SEPARATOR . 'lockfile.txt');
?>
--EXPECT--
string(1) "1"
string(1) "a"
string(1) "2"
string(1) "b"
Done
