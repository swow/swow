<?php
require __DIR__ . '/../include/bootstrap.php';

use Swow\Socket;

const TEST_LOCKNAME = __DIR__ . DIRECTORY_SEPARATOR . "lockfile.txt";

$fd = fopen(TEST_LOCKNAME, "c+");
if(!$fd){
    fprintf(STDERR, "failed open lock file\n");
    exit(1);
}

// argv[1] is pipe name
// connect it
$sock = new Socket(Socket::TYPE_PIPE);
//printf("wait connecting\n");
$conn = $sock->connect($argv[1]);
//printf("connected\n");

function tellpar($char){
    global $conn;
    $conn->sendString($char);
}
function waitpar($char){
    global $conn;
    $red = $conn->readString(1);
    var_dump($char);
    if(strncmp($char, $red, 1) !=0){
        throw new Exception("parent failed continue");
    }
}

// wait parent instruction
waitpar("1");

// lock target
$ret = flock($fd, LOCK_EX);
if(!$ret){
    fprintf(STDERR, "failed lock file\n");
    exit(1);
}
// tell parent continue
tellpar("a");

// wait parent instruction
waitpar("2");

// unlock file
flock($fd, LOCK_UN);

tellpar("b");

// clean up and exit
$conn->close();
fclose($fd);
