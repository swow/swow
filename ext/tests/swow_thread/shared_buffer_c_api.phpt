--TEST--
swow_thread: SharedBuffer C interface
--SKIPIF--
<?php
require __DIR__ . '/../include/skipif.php';
skip_if_extension_not_exist('ffi');
skip_if_c_function_not_exist('int swow_shared_buffer_last_error_code(void);');
?>
--FILE--
<?php
require __DIR__ . '/../include/bootstrap.php';

$ffi = FFI::cdef(<<<'DEF'
typedef struct swow_shared_buffer_s swow_shared_buffer_t;
typedef struct swow_shared_buffer_header_s {
    char magic[4];
    unsigned short version;
    unsigned short flags;
    unsigned int refcount;
    unsigned int data_offset;
    unsigned long long size;
    unsigned long long alignment;
    unsigned int header_size;
    unsigned int reserved0;
    unsigned long long reserved[3];
} swow_shared_buffer_header_t;
typedef struct cat_shm_s cat_shm_t;

swow_shared_buffer_t *swow_shared_buffer_create(const char *name, size_t size, size_t alignment, unsigned int flags);
swow_shared_buffer_t *swow_shared_buffer_attach(const char *name);
void swow_shared_buffer_addref(swow_shared_buffer_t *buffer);
void swow_shared_buffer_release(swow_shared_buffer_t *buffer);
int swow_shared_buffer_unlink(const char *name);
void *swow_shared_buffer_get_data(const swow_shared_buffer_t *buffer);
size_t swow_shared_buffer_get_size(const swow_shared_buffer_t *buffer);
size_t swow_shared_buffer_get_alignment(const swow_shared_buffer_t *buffer);
const char *swow_shared_buffer_get_name(const swow_shared_buffer_t *buffer);
swow_shared_buffer_header_t *swow_shared_buffer_get_header(const swow_shared_buffer_t *buffer);
unsigned int swow_shared_buffer_get_refcount(const swow_shared_buffer_t *buffer);
swow_shared_buffer_t *swow_shared_buffer_slice(swow_shared_buffer_t *buffer, size_t offset, size_t size);
int swow_shared_buffer_last_error_code(void);
const char *swow_shared_buffer_last_error_message(void);

cat_shm_t *cat_shm_create_named(const char *name, size_t size);
cat_shm_t *cat_shm_attach_named(const char *name);
void cat_shm_detach(cat_shm_t *shm);
int cat_shm_unlink(const char *name);
DEF);

const EINVAL = -22;
const EBUSY = -16;
const ENOENT = -2;

$lastError = static fn(): array => [
    $ffi->swow_shared_buffer_last_error_code(),
    $ffi->swow_shared_buffer_last_error_message(),
];

$id = getmypid() . '-' . bin2hex(random_bytes(4));
$name = 'sbuf-' . $id;
$slashName = '/' . $name;
$tinyName = 'tiny-' . $id;
$zeroName = 'zero-' . $id;
$missingName = 'missing-' . $id;
$autoName = 'auto-' . $id;
$leakName = 'leak-' . $id;
$keepName = 'keep-' . $id;

try {
    # create validation
    Assert::null($ffi->swow_shared_buffer_create(null, 0, 64, 0));
    Assert::same([EINVAL, 'The shared buffer size must be positive'], $lastError());
    foreach ([0, 4, 7, 48, 8192] as $alignment) {
        Assert::null($ffi->swow_shared_buffer_create(null, 8, $alignment, 0));
        Assert::same(EINVAL, $ffi->swow_shared_buffer_last_error_code());
    }
    Assert::null($ffi->swow_shared_buffer_create(null, 8, 64, 65536));
    Assert::same([EINVAL, 'The flags do not fit the header'], $lastError());
    Assert::null($ffi->swow_shared_buffer_create('', 8, 64, 0));
    Assert::same([EINVAL, 'The name must not be empty'], $lastError());
    Assert::null($ffi->swow_shared_buffer_create(null, -1, 64, 0));
    Assert::same([EINVAL, 'The shared buffer is too large'], $lastError());

    # the platform bounds the name length: Linux NAME_MAX (255), macOS 31 with
    # the leading slash, and Windows MAX_PATH (260)
    $nameLimit = PHP_OS_FAMILY === 'Darwin' ? 30 : (PHP_OS_FAMILY === 'Windows' ? 260 : 255);
    Assert::throws(static function () use ($nameLimit): void {
        Swow\SharedBuffer::createNamed(str_repeat('n', $nameLimit + 1), 32);
    }, Swow\SyncException::class, null, '/longer than/');

    # a name with a leading slash goes through the POSIX path branch
    $root = $ffi->swow_shared_buffer_create($slashName, 32, 128, 0);
    Assert::true($root !== null);
    Assert::same($slashName, $ffi->swow_shared_buffer_get_name($root));
    Assert::same(32, $ffi->swow_shared_buffer_get_size($root));
    Assert::same(128, $ffi->swow_shared_buffer_get_alignment($root));
    Assert::true($ffi->swow_shared_buffer_get_data($root) !== null);
    $header = $ffi->swow_shared_buffer_get_header($root);
    Assert::same('SWBS', FFI::string($header->magic, 4));
    Assert::same(1, $header->version);
    Assert::same(128, $header->data_offset);
    Assert::same(32, $header->size);
    Assert::same(128, $header->alignment);
    Assert::same(64, $header->header_size);
    Assert::same(1, $header->refcount);
    Assert::same(1, $ffi->swow_shared_buffer_get_refcount($root));
    Assert::same(1, $ffi->swow_shared_buffer_unlink($slashName));
    Assert::null($ffi->swow_shared_buffer_attach($slashName));
    Assert::same(ENOENT, $ffi->swow_shared_buffer_last_error_code());
    $ffi->swow_shared_buffer_release($root);

    # an anonymous buffer has no name
    $anon = $ffi->swow_shared_buffer_create(null, 16, 64, 0);
    Assert::true($anon !== null);
    Assert::null($ffi->swow_shared_buffer_get_name($anon));
    Assert::same(16, $ffi->swow_shared_buffer_get_size($anon));
    $ffi->swow_shared_buffer_release($anon);

    # attach errors; macOS maps a shared memory object by page, so a small
    # object reports a page-sized mapping, the size check passes there, and the
    # header check rejects it
    $tiny = $ffi->cat_shm_create_named($tinyName, 12);
    Assert::true($tiny !== null);
    Assert::null($ffi->swow_shared_buffer_attach($tinyName));
    $tinyError = PHP_OS_FAMILY === 'Darwin'
        ? [EBUSY, 'The shared buffer header is incomplete']
        : [EINVAL, 'The mapping is too small for a shared buffer'];
    Assert::same($tinyError, $lastError());
    $ffi->cat_shm_detach($tiny);
    Assert::same(1, $ffi->cat_shm_unlink($tinyName));

    Assert::null($ffi->swow_shared_buffer_attach($missingName));
    Assert::same(ENOENT, $ffi->swow_shared_buffer_last_error_code());
    Assert::null($ffi->swow_shared_buffer_attach(null));
    Assert::same([EINVAL, 'The name must not be empty'], $lastError());
    Assert::null($ffi->swow_shared_buffer_attach(''));
    Assert::same([EINVAL, 'The name must not be empty'], $lastError());

    # an empty mapping is rejected before the header is read. A raw shm_open
    # through PHP FFI on macOS creates the object with a mode that denies the
    # next open, so the case runs where FFI can make an attachable object; the
    # C tests cover the empty object on macOS
    if (PHP_OS_FAMILY !== 'Darwin') {
        $libc = FFI::cdef('int shm_open(const char *, int, unsigned int); int shm_unlink(const char *); int close(int);');
        $fd = $libc->shm_open('/' . $zeroName, 0x40 /* O_CREAT */ | 0x2 /* O_RDWR */, 0600);
        Assert::true($fd >= 0);
        Assert::null($ffi->swow_shared_buffer_attach($zeroName));
        Assert::same([EINVAL, 'The size of a shared mapping is out of range'], $lastError());
        $libc->close($fd);
        $libc->shm_unlink('/' . $zeroName);
    }

    # a header that is not complete reports EBUSY
    $named = $ffi->swow_shared_buffer_create($name, 32, 64, 0);
    Assert::true($named !== null);
    $header = $ffi->swow_shared_buffer_get_header($named);
    FFI::memset($header->magic, 0, 4);
    Assert::null($ffi->swow_shared_buffer_attach($name));
    Assert::same([EBUSY, 'The shared buffer header is incomplete'], $lastError());
    FFI::memcpy($header->magic, 'SWBS', 4);
    $header->version = 2;
    Assert::null($ffi->swow_shared_buffer_attach($name));
    Assert::same([EBUSY, 'The shared buffer header is incomplete'], $lastError());
    $header->version = 1;

    # a header whose fields contradict the mapping reports EINVAL
    $header->data_offset = 63;
    Assert::null($ffi->swow_shared_buffer_attach($name));
    Assert::same([EINVAL, 'The shared buffer header is not valid'], $lastError());
    $header->data_offset = 0;
    Assert::null($ffi->swow_shared_buffer_attach($name));
    Assert::same([EINVAL, 'The shared buffer header is not valid'], $lastError());
    $header->data_offset = 64;
    # the values reach beyond any page-rounded mapping, because macOS maps an
    # object by page and the mapping can exceed the object size
    $header->size = 1 << 20;
    Assert::null($ffi->swow_shared_buffer_attach($name));
    Assert::same([EINVAL, 'The shared buffer does not fit the mapping'], $lastError());
    $header->size = 32;
    $header->data_offset = 1 << 20;
    Assert::null($ffi->swow_shared_buffer_attach($name));
    Assert::same([EINVAL, 'The shared buffer does not fit the mapping'], $lastError());
    $header->data_offset = 64;

    # a valid header attaches and bumps the mapping refcount
    $attached = $ffi->swow_shared_buffer_attach($name);
    Assert::true($attached !== null);
    Assert::same($name, $ffi->swow_shared_buffer_get_name($attached));
    Assert::same(32, $ffi->swow_shared_buffer_get_size($attached));
    Assert::same(2, $ffi->swow_shared_buffer_get_refcount($named));
    $ffi->swow_shared_buffer_addref($attached);
    Assert::same(2, $ffi->swow_shared_buffer_get_refcount($attached));

    # a slice holds a handle reference and shares the data area
    $parentData = $ffi->swow_shared_buffer_get_data($named);
    FFI::memcpy($parentData, str_repeat("\0", 8) . 'slice', 13);
    $slice = $ffi->swow_shared_buffer_slice($named, 8, 16);
    Assert::true($slice !== null);
    Assert::same(16, $ffi->swow_shared_buffer_get_size($slice));
    Assert::same(64, $ffi->swow_shared_buffer_get_alignment($slice));
    Assert::null($ffi->swow_shared_buffer_get_name($slice));
    Assert::same(2, $ffi->swow_shared_buffer_get_refcount($slice));
    Assert::same('slice', FFI::string($ffi->swow_shared_buffer_get_data($slice), 5));
    $ffi->swow_shared_buffer_release($slice);
    Assert::null($ffi->swow_shared_buffer_slice($named, 32, 1));
    Assert::same([EINVAL, 'The slice is outside the shared buffer'], $lastError());
    Assert::null($ffi->swow_shared_buffer_slice($named, 0, 33));
    Assert::same([EINVAL, 'The slice is outside the shared buffer'], $lastError());
    Assert::null($ffi->swow_shared_buffer_slice(null, 0, 0));
    Assert::same([EINVAL, 'The shared buffer is required'], $lastError());

    # getters accept NULL, and the last_error functions report the failure
    Assert::null($ffi->swow_shared_buffer_get_data(null));
    Assert::same(0, $ffi->swow_shared_buffer_get_size(null));
    Assert::same(0, $ffi->swow_shared_buffer_get_alignment(null));
    Assert::null($ffi->swow_shared_buffer_get_name(null));
    Assert::null($ffi->swow_shared_buffer_get_header(null));
    Assert::same(0, $ffi->swow_shared_buffer_get_refcount(null));
    $ffi->swow_shared_buffer_addref(null);
    $ffi->swow_shared_buffer_release(null);
    Assert::same(EINVAL, $ffi->swow_shared_buffer_last_error_code());
    Assert::true($ffi->swow_shared_buffer_last_error_message() !== '');

    # unlink errors
    Assert::same(0, $ffi->swow_shared_buffer_unlink(null));
    Assert::same([EINVAL, 'The name must not be empty'], $lastError());
    Assert::same(0, $ffi->swow_shared_buffer_unlink(''));
    Assert::same([EINVAL, 'The name must not be empty'], $lastError());
    if (PHP_OS_FAMILY === 'Windows') {
        /* a named section ends with its last handle, so a missing name is not an error */
        Assert::same(1, $ffi->swow_shared_buffer_unlink($missingName));
    } else {
        Assert::same(0, $ffi->swow_shared_buffer_unlink($missingName));
        Assert::same(ENOENT, $ffi->swow_shared_buffer_last_error_code());
    }

    # the libcat layer rejects its own bad arguments
    Assert::null($ffi->cat_shm_create_named('', 8));
    Assert::same([EINVAL, 'The name of a shared mapping is empty'], $lastError());
    Assert::null($ffi->cat_shm_create_named($missingName, 0));
    Assert::same([EINVAL, 'The size of a shared mapping is out of range'], $lastError());
    Assert::null($ffi->cat_shm_attach_named(''));
    Assert::same([EINVAL, 'The name of a shared mapping is empty'], $lastError());
    Assert::null($ffi->cat_shm_attach_named($missingName));
    Assert::same(ENOENT, $ffi->swow_shared_buffer_last_error_code());
    Assert::same(0, $ffi->cat_shm_unlink(''));
    Assert::same([EINVAL, 'The name of a shared mapping is empty'], $lastError());

    # a size that the file system can not represent fails after the open
    Assert::null($ffi->cat_shm_create_named('huge-' . $id, -1));
    Assert::same(EINVAL, $ffi->swow_shared_buffer_last_error_code());
    Assert::true(str_contains($ffi->swow_shared_buffer_last_error_message(), 'ftruncate'));

    # the last mapping of an unlink-on-last buffer removes the name
    $auto = $ffi->swow_shared_buffer_create($autoName, 8, 64, 1 /* SWOW_SHARED_BUFFER_FLAG_UNLINK_ON_LAST */);
    Assert::true($auto !== null);
    $autoAttached = $ffi->swow_shared_buffer_attach($autoName);
    Assert::same(2, $ffi->swow_shared_buffer_get_refcount($auto));
    $ffi->swow_shared_buffer_release($autoAttached);
    Assert::same(1, $ffi->swow_shared_buffer_get_refcount($auto));
    $ffi->swow_shared_buffer_release($auto);
    Assert::null($ffi->swow_shared_buffer_attach($autoName));
    Assert::same(ENOENT, $ffi->swow_shared_buffer_last_error_code());

    $ffi->swow_shared_buffer_release($attached);
    $ffi->swow_shared_buffer_release($named);
    Assert::same(1, $ffi->cat_shm_unlink($name));

    # leak one handle for the module shutdown sweep and one for its skip branch
    $leak = $ffi->swow_shared_buffer_create($leakName, 8, 64, 0);
    Assert::true($leak !== null);
    $keep = $ffi->swow_shared_buffer_create($keepName, 8, 64, 0);
    Assert::true($keep !== null);
    $ffi->swow_shared_buffer_addref($keep);
    Assert::same(1, $ffi->cat_shm_unlink($leakName));
    Assert::same(1, $ffi->cat_shm_unlink($keepName));
} finally {
    foreach ([$name, $slashName, $tinyName, $autoName, $leakName, $keepName] as $leftover) {
        try {
            $ffi->swow_shared_buffer_unlink($leftover);
        } catch (Throwable) {
        }
    }
}

echo "Done\n";
?>
--EXPECT--
Done
