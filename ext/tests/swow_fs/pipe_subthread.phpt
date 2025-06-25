--TEST--
swow_fs: pipe subthread
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';

skip_if_not_zts();
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Siritz;
use function Swow\pipe;
use function Swow\fileno;
use function Swow\pipe_from_fd;

[$r, $w] = pipe();
$rfd = fileno($r);
$wfd = fileno($w);

$cb = function () use ($rfd, $wfd) {
    $r = pipe_from_fd($rfd, "rb");
    $w = pipe_from_fd($wfd, "wb");

    fwrite($w, 'Hello from child');
    $data = fread($r, 1024);
    var_dump($data);
    fclose($r);
    fclose($w);
};

$thread = new Siritz($cb);

$thread->run();

$data = fread($r, 1024);
var_dump($data);
fwrite($w, 'Hello from parent');
fclose($r);
fclose($w);

$thread->wait();

echo 'Done' . PHP_EOL;
?>
--EXPECT--
string(16) "Hello from child"
string(17) "Hello from parent"
Done
