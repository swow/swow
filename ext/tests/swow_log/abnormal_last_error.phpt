--TEST--
swow_log: abnormal log does not free the message owned by last_error
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if(!Swow\Extension::isBuiltWith('openssl'), 'extension must be built with openssl');
skip_if_c_function_not_exist('
    void ERR_new(void);
    void ERR_set_debug(const char *file, int line, const char *func);
    void ERR_set_error(int lib, int reason, const char *fmt, ...);
    void ERR_set_error_data(char *data, int flags);
');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

// The double free corrupts the heap in the child, but whether the allocator
// notices depends on the allocation history of the current process. Run the
// scenario in a fresh process and assert its exit status instead.
$paths = testX509Paths(__DIR__ . '/abnormal_last_error_x509');

$child = tempnam(sys_get_temp_dir(), 'swow_log_abnormal_');
file_put_contents($child, <<<'PHP'
<?php

use Swow\Coroutine;
use Swow\Socket;

set_error_handler(static fn(): bool => true);

[$cert, $key] = [$argv[1], $argv[2]];

$server = new Socket(Socket::TYPE_TCP);
$server->bind('127.0.0.1', 0);
$port = $server->getSockPort();
$server->listen();

Coroutine::run(static function () use ($server, $cert, $key): void {
    $connection = $server->accept();
    $connection->enableCrypto([
        'certificate' => $cert,
        'certificate_key' => $key,
        'verify_peer' => false,
    ]);
    $connection->send('hello');
    $connection->close();
});

// Leave a stale error in this thread's OpenSSL error queue with a large data
// payload, so the abnormal log builds a large message. After that message is
// handed to last_error and freed, the block is returned to the OS and cannot be
// recycled before cat_clear_last_error() frees it again at request shutdown.
$ffi = FFI::cdef('
    void ERR_new(void);
    void ERR_set_debug(const char *file, int line, const char *func);
    void ERR_set_error(int lib, int reason, const char *fmt, ...);
    void ERR_set_error_data(char *data, int flags);
');
$data = str_repeat('x', 200000);
$buffer = FFI::new('char[' . (strlen($data) + 1) . ']');
FFI::memcpy($buffer, $data, strlen($data));
$ffi->ERR_new();
$ffi->ERR_set_debug('abnormal_last_error.phpt', 1, 'test');
$ffi->ERR_set_error(20 /* ERR_LIB_SSL */, 128 /* SSL_R_BIO_NOT_SET */, null);
$ffi->ERR_set_error_data(FFI::cast('char *', $buffer), 2 /* ERR_TXT_STRING */);

$client = new Socket(Socket::TYPE_TCP);
$client->connect('127.0.0.1', $port);
$client->enableCrypto([
    'verify_peer' => false,
    'verify_peer_name' => false,
]);
if ($client->recvString(1024) !== 'hello') {
    echo "unexpected payload\n";
}
$client->close();

// Reaching request shutdown with last_error still populated exercises
// cat_clear_last_error(); it must not free an already freed message.
echo "Done\n";
PHP
);

$env = getenv();
$env['USE_ZEND_ALLOC'] = '0';

$command = [
    PHP_BINARY,
    '-n',
    '-d', 'extension_dir=' . ini_get('extension_dir'),
    '-d', 'extension=swow',
    '-d', 'display_errors=0',
    $child,
    $paths['localhost']['cert'],
    $paths['localhost']['key'],
];
$process = proc_open($command, [1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $pipes, null, $env);
$output = stream_get_contents($pipes[1]);
$errors = stream_get_contents($pipes[2]);
fclose($pipes[1]);
fclose($pipes[2]);
$exitCode = proc_close($process);
unlink($child);

if ($exitCode !== 0) {
    echo 'child exited with code ', $exitCode, "\n", $output, $errors;
} else {
    echo "Done\n";
}

?>
--EXPECT--
Done
