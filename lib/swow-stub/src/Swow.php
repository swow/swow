<?php

/** @noinspection PhpUnused, PhpInconsistentReturnPointsInspection, PhpMissingParentConstructorInspection, PhpReturnDocTypeMismatchInspection */

namespace
{
    class Swow
    {
        public const MAJOR_VERSION = 0;
        public const MINOR_VERSION = 1;
        public const RELEASE_VERSION = 0;
        public const EXTRA_VERSION = '';
        public const VERSION = '0.1.0';
        public const VERSION_ID = 100;

        public static function isBuiltWith(string $lib): bool { }
    }
}

namespace
{
    function msleep(int $milli_seconds): int { }
}

namespace Swow
{
    class Module
    {
        public const TYPE_CORE = 1;
        public const TYPE_COROUTINE = 2;
        public const TYPE_CHANNEL = 4;
        public const TYPE_SYNC = 8;
        public const TYPE_EVENT = 16;
        public const TYPE_TIME = 32;
        public const TYPE_SOCKET = 64;
        public const TYPE_DNS = 128;
        public const TYPE_WORK = 256;
        public const TYPE_BUFFER = 512;
        public const TYPE_FS = 1024;
        public const TYPE_SIGNAL = 2048;
        public const TYPE_PROCESS = 4096;
        public const TYPE_THREAD = 8192;
        public const TYPE_OS = 16384;
        public const TYPE_WATCH_DOG = 131072;
        public const TYPE_PROTOCOL = 262144;
        public const TYPE_SSL = 2097152;
        public const TYPE_EXT = 4194304;
        public const TYPE_TEST = 8388608;
        public const TYPE_USR1 = 16777216;
        public const TYPE_USR2 = 33554432;
        public const TYPE_USR3 = 67108864;
        public const TYPE_USR4 = 134217728;
        public const TYPE_USR5 = 268435456;
        public const TYPE_USR6 = 536870912;
        public const TYPE_USR7 = 1073741824;
        public const TYPE_USR8 = -2147483648;
        public const TYPES_BUILTIN = 15106047;
        public const TYPES_USR = -16777216;
        public const TYPES_ALL = -1671169;
    }
}

namespace Swow
{
    class Errno
    {
        /**
         * This constant holds UV_E2BIG value, it's platform-dependent.
         *
         * At Linux and macOS platforms, this constant means "Argument list too long"
         * At Windows platform, this constant may have a value `-4093`
         */
        public const E2BIG = -7;
        /**
         * This constant holds UV_EACCES value, it's platform-dependent.
         *
         * At Linux and macOS platforms, this constant means "Permission denied"
         * At Windows platform, this constant may have a value `-4092`
         */
        public const EACCES = -13;
        /**
         * This constant holds UV_EADDRINUSE value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Address already in use"
         * At macOS platform, this constant may have a value `-48` means "Address already in use"
         * At Windows platform, this constant may have a value `-4091`
         */
        public const EADDRINUSE = -98;
        /**
         * This constant holds UV_EADDRNOTAVAIL value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Cannot assign requested address"
         * At macOS platform, this constant may have a value `-49` means "Can't assign requested address"
         * At Windows platform, this constant may have a value `-4090`
         */
        public const EADDRNOTAVAIL = -99;
        /**
         * This constant holds UV_EAFNOSUPPORT value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Address family not supported by protocol"
         * At macOS platform, this constant may have a value `-47` means "Address family not supported by protocol family"
         * At Windows platform, this constant may have a value `-4089`
         */
        public const EAFNOSUPPORT = -97;
        /**
         * This constant holds UV_EAGAIN value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Try again"
         * At macOS platform, this constant may have a value `-35` means "Resource temporarily unavailable"
         * At Windows platform, this constant may have a value `-4088`
         */
        public const EAGAIN = -11;
        public const EAI_ADDRFAMILY = -3000;
        public const EAI_AGAIN = -3001;
        public const EAI_BADFLAGS = -3002;
        public const EAI_BADHINTS = -3013;
        public const EAI_CANCELED = -3003;
        public const EAI_FAIL = -3004;
        public const EAI_FAMILY = -3005;
        public const EAI_MEMORY = -3006;
        public const EAI_NODATA = -3007;
        public const EAI_NONAME = -3008;
        public const EAI_OVERFLOW = -3009;
        public const EAI_PROTOCOL = -3014;
        public const EAI_SERVICE = -3010;
        public const EAI_SOCKTYPE = -3011;
        /**
         * This constant holds UV_EALREADY value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Operation already in progress"
         * At macOS platform, this constant may have a value `-37` means "Operation already in progress"
         * At Windows platform, this constant may have a value `-4084`
         */
        public const EALREADY = -114;
        /**
         * This constant holds UV_EBADF value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Bad file number"
         * At macOS platform, this constant means "Bad file descriptor"
         * At Windows platform, this constant may have a value `-4083`
         */
        public const EBADF = -9;
        /**
         * This constant holds UV_EBUSY value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Device or resource busy"
         * At macOS platform, this constant means "Device / Resource busy"
         * At Windows platform, this constant may have a value `-4082`
         */
        public const EBUSY = -16;
        /**
         * This constant holds UV_ECANCELED value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Operation Canceled"
         * At macOS platform, this constant may have a value `-89` means "Operation canceled"
         * At Windows platform, this constant may have a value `-4081`
         */
        public const ECANCELED = -125;
        public const ECHARSET = -4080;
        /**
         * This constant holds UV_ECONNABORTED value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Software caused connection abort"
         * At macOS platform, this constant may have a value `-53` means "Software caused connection abort"
         * At Windows platform, this constant may have a value `-4079`
         */
        public const ECONNABORTED = -103;
        /**
         * This constant holds UV_ECONNREFUSED value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Connection refused"
         * At macOS platform, this constant may have a value `-61` means "Connection refused"
         * At Windows platform, this constant may have a value `-4078`
         */
        public const ECONNREFUSED = -111;
        /**
         * This constant holds UV_ECONNRESET value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Connection reset by peer"
         * At macOS platform, this constant may have a value `-54` means "Connection reset by peer"
         * At Windows platform, this constant may have a value `-4077`
         */
        public const ECONNRESET = -104;
        /**
         * This constant holds UV_EDESTADDRREQ value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Destination address required"
         * At macOS platform, this constant may have a value `-39` means "Destination address required"
         * At Windows platform, this constant may have a value `-4076`
         */
        public const EDESTADDRREQ = -89;
        /**
         * This constant holds UV_EEXIST value, it's platform-dependent.
         *
         * At Linux and macOS platforms, this constant means "File exists"
         * At Windows platform, this constant may have a value `-4075`
         */
        public const EEXIST = -17;
        /**
         * This constant holds UV_EFAULT value, it's platform-dependent.
         *
         * At Linux and macOS platforms, this constant means "Bad address"
         * At Windows platform, this constant may have a value `-4074`
         */
        public const EFAULT = -14;
        /**
         * This constant holds UV_EFBIG value, it's platform-dependent.
         *
         * At Linux and macOS platforms, this constant means "File too large"
         * At Windows platform, this constant may have a value `-4036`
         */
        public const EFBIG = -27;
        /**
         * This constant holds UV_EHOSTUNREACH value, it's platform-dependent.
         *
         * At Linux platform, this constant means "No route to host"
         * At macOS platform, this constant may have a value `-65` means "No route to host"
         * At Windows platform, this constant may have a value `-4073`
         */
        public const EHOSTUNREACH = -113;
        /**
         * This constant holds UV_EINTR value, it's platform-dependent.
         *
         * At Linux and macOS platforms, this constant means "Interrupted system call"
         * At Windows platform, this constant may have a value `-4072`
         */
        public const EINTR = -4;
        /**
         * This constant holds UV_EINVAL value, it's platform-dependent.
         *
         * At Linux and macOS platforms, this constant means "Invalid argument"
         * At Windows platform, this constant may have a value `-4071`
         */
        public const EINVAL = -22;
        /**
         * This constant holds UV_EIO value, it's platform-dependent.
         *
         * At Linux platform, this constant means "I/O error"
         * At macOS platform, this constant means "Input/output error"
         * At Windows platform, this constant may have a value `-4070`
         */
        public const EIO = -5;
        /**
         * This constant holds UV_EISCONN value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Transport endpoint is already connected"
         * At macOS platform, this constant may have a value `-56` means "Socket is already connected"
         * At Windows platform, this constant may have a value `-4069`
         */
        public const EISCONN = -106;
        /**
         * This constant holds UV_EISDIR value, it's platform-dependent.
         *
         * At Linux and macOS platforms, this constant means "Is a directory"
         * At Windows platform, this constant may have a value `-4068`
         */
        public const EISDIR = -21;
        /**
         * This constant holds UV_ELOOP value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Too many symbolic links encountered"
         * At macOS platform, this constant may have a value `-62` means "Too many levels of symbolic links"
         * At Windows platform, this constant may have a value `-4067`
         */
        public const ELOOP = -40;
        /**
         * This constant holds UV_EMFILE value, it's platform-dependent.
         *
         * At Linux and macOS platforms, this constant means "Too many open files"
         * At Windows platform, this constant may have a value `-4066`
         */
        public const EMFILE = -24;
        /**
         * This constant holds UV_EMSGSIZE value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Message too long"
         * At macOS platform, this constant may have a value `-40` means "Message too long"
         * At Windows platform, this constant may have a value `-4065`
         */
        public const EMSGSIZE = -90;
        /**
         * This constant holds UV_ENAMETOOLONG value, it's platform-dependent.
         *
         * At Linux platform, this constant means "File name too long"
         * At macOS platform, this constant may have a value `-63` means "File name too long"
         * At Windows platform, this constant may have a value `-4064`
         */
        public const ENAMETOOLONG = -36;
        /**
         * This constant holds UV_ENETDOWN value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Network is down"
         * At macOS platform, this constant may have a value `-50` means "Network is down"
         * At Windows platform, this constant may have a value `-4063`
         */
        public const ENETDOWN = -100;
        /**
         * This constant holds UV_ENETUNREACH value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Network is unreachable"
         * At macOS platform, this constant may have a value `-51` means "Network is unreachable"
         * At Windows platform, this constant may have a value `-4062`
         */
        public const ENETUNREACH = -101;
        /**
         * This constant holds UV_ENFILE value, it's platform-dependent.
         *
         * At Linux platform, this constant means "File table overflow"
         * At macOS platform, this constant means "Too many open files in system"
         * At Windows platform, this constant may have a value `-4061`
         */
        public const ENFILE = -23;
        /**
         * This constant holds UV_ENOBUFS value, it's platform-dependent.
         *
         * At Linux platform, this constant means "No buffer space available"
         * At macOS platform, this constant may have a value `-55` means "No buffer space available"
         * At Windows platform, this constant may have a value `-4060`
         */
        public const ENOBUFS = -105;
        /**
         * This constant holds UV_ENODEV value, it's platform-dependent.
         *
         * At Linux platform, this constant means "No such device"
         * At macOS platform, this constant means "Operation not supported by device"
         * At Windows platform, this constant may have a value `-4059`
         */
        public const ENODEV = -19;
        /**
         * This constant holds UV_ENOENT value, it's platform-dependent.
         *
         * At Linux and macOS platforms, this constant means "No such file or directory"
         * At Windows platform, this constant may have a value `-4058`
         */
        public const ENOENT = -2;
        /**
         * This constant holds UV_ENOMEM value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Out of memory"
         * At macOS platform, this constant means "Cannot allocate memory"
         * At Windows platform, this constant may have a value `-4057`
         */
        public const ENOMEM = -12;
        /**
         * This constant holds UV_ENONET value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Machine is not on the network"
         * At macOS and Windows platforms, this constant may have a value `-4056`
         */
        public const ENONET = -64;
        /**
         * This constant holds UV_ENOPROTOOPT value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Protocol not available"
         * At macOS platform, this constant may have a value `-42` means "Protocol not available"
         * At Windows platform, this constant may have a value `-4035`
         */
        public const ENOPROTOOPT = -92;
        /**
         * This constant holds UV_ENOSPC value, it's platform-dependent.
         *
         * At Linux and macOS platforms, this constant means "No space left on device"
         * At Windows platform, this constant may have a value `-4055`
         */
        public const ENOSPC = -28;
        /**
         * This constant holds UV_ENOSYS value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Invalid system call number"
         * At macOS platform, this constant may have a value `-78` means "Function not implemented"
         * At Windows platform, this constant may have a value `-4054`
         */
        public const ENOSYS = -38;
        /**
         * This constant holds UV_ENOTCONN value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Transport endpoint is not connected"
         * At macOS platform, this constant may have a value `-57` means "Socket is not connected"
         * At Windows platform, this constant may have a value `-4053`
         */
        public const ENOTCONN = -107;
        /**
         * This constant holds UV_ENOTDIR value, it's platform-dependent.
         *
         * At Linux and macOS platforms, this constant means "Not a directory"
         * At Windows platform, this constant may have a value `-4052`
         */
        public const ENOTDIR = -20;
        /**
         * This constant holds UV_ENOTEMPTY value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Directory not empty"
         * At macOS platform, this constant may have a value `-66` means "Directory not empty"
         * At Windows platform, this constant may have a value `-4051`
         */
        public const ENOTEMPTY = -39;
        /**
         * This constant holds UV_ENOTSOCK value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Socket operation on non-socket"
         * At macOS platform, this constant may have a value `-38` means "Socket operation on non-socket"
         * At Windows platform, this constant may have a value `-4050`
         */
        public const ENOTSOCK = -88;
        /**
         * This constant holds UV_ENOTSUP value, it's platform-dependent.
         *
         * At macOS platform, this constant may have a value `-45` means "Operation not supported"
         */
        public const ENOTSUP = -4049;
        /**
         * This constant holds UV_EOVERFLOW value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Value too large for defined data type"
         * At macOS platform, this constant may have a value `-84` means "Value too large to be stored in data type"
         * At Windows platform, this constant may have a value `-4026`
         */
        public const EOVERFLOW = -75;
        /**
         * This constant holds UV_EPERM value, it's platform-dependent.
         *
         * At Linux and macOS platforms, this constant means "Operation not permitted"
         * At Windows platform, this constant may have a value `-4048`
         */
        public const EPERM = -1;
        /**
         * This constant holds UV_EPIPE value, it's platform-dependent.
         *
         * At Linux and macOS platforms, this constant means "Broken pipe"
         * At Windows platform, this constant may have a value `-4047`
         */
        public const EPIPE = -32;
        /**
         * This constant holds UV_EPROTO value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Protocol error"
         * At macOS platform, this constant may have a value `-100` means "Protocol error"
         * At Windows platform, this constant may have a value `-4046`
         */
        public const EPROTO = -71;
        /**
         * This constant holds UV_EPROTONOSUPPORT value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Protocol not supported"
         * At macOS platform, this constant may have a value `-43` means "Protocol not supported"
         * At Windows platform, this constant may have a value `-4045`
         */
        public const EPROTONOSUPPORT = -93;
        /**
         * This constant holds UV_EPROTOTYPE value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Protocol wrong type for socket"
         * At macOS platform, this constant may have a value `-41` means "Protocol wrong type for socket"
         * At Windows platform, this constant may have a value `-4044`
         */
        public const EPROTOTYPE = -91;
        /**
         * This constant holds UV_ERANGE value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Math result not representable"
         * At macOS platform, this constant means "Result too large"
         * At Windows platform, this constant may have a value `-4034`
         */
        public const ERANGE = -34;
        /**
         * This constant holds UV_EROFS value, it's platform-dependent.
         *
         * At Linux and macOS platforms, this constant means "Read-only file system"
         * At Windows platform, this constant may have a value `-4043`
         */
        public const EROFS = -30;
        /**
         * This constant holds UV_ESHUTDOWN value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Cannot send after transport endpoint shutdown"
         * At macOS platform, this constant may have a value `-58` means "Can't send after socket shutdown"
         * At Windows platform, this constant may have a value `-4042`
         */
        public const ESHUTDOWN = -108;
        /**
         * This constant holds UV_ESPIPE value, it's platform-dependent.
         *
         * At Linux and macOS platforms, this constant means "Illegal seek"
         * At Windows platform, this constant may have a value `-4041`
         */
        public const ESPIPE = -29;
        /**
         * This constant holds UV_ESRCH value, it's platform-dependent.
         *
         * At Linux and macOS platforms, this constant means "No such process"
         * At Windows platform, this constant may have a value `-4040`
         */
        public const ESRCH = -3;
        /**
         * This constant holds UV_ETIMEDOUT value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Connection timed out"
         * At macOS platform, this constant may have a value `-60` means "Operation timed out"
         * At Windows platform, this constant may have a value `-4039`
         */
        public const ETIMEDOUT = -110;
        /**
         * This constant holds UV_ETXTBSY value, it's platform-dependent.
         *
         * At Linux and macOS platforms, this constant means "Text file busy"
         * At Windows platform, this constant may have a value `-4038`
         */
        public const ETXTBSY = -26;
        /**
         * This constant holds UV_EXDEV value, it's platform-dependent.
         *
         * At Linux and macOS platforms, this constant means "Cross-device link"
         * At Windows platform, this constant may have a value `-4037`
         */
        public const EXDEV = -18;
        public const UNKNOWN = -4094;
        public const EOF = -4095;
        /**
         * This constant holds UV_ENXIO value, it's platform-dependent.
         *
         * At Linux platform, this constant means "No such device or address"
         * At macOS platform, this constant means "Device not configured"
         * At Windows platform, this constant may have a value `-4033`
         */
        public const ENXIO = -6;
        /**
         * This constant holds UV_EMLINK value, it's platform-dependent.
         *
         * At Linux and macOS platforms, this constant means "Too many links"
         * At Windows platform, this constant may have a value `-4032`
         */
        public const EMLINK = -31;
        /**
         * This constant holds UV_EHOSTDOWN value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Host is down"
         * At macOS platform, this constant may have a value `-64` means "Host is down"
         * At Windows platform, this constant may have a value `-4031`
         */
        public const EHOSTDOWN = -112;
        /**
         * This constant holds UV_EREMOTEIO value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Remote I/O error"
         * At macOS and Windows platforms, this constant may have a value `-4030`
         */
        public const EREMOTEIO = -121;
        /**
         * This constant holds UV_ENOTTY value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Not a typewriter"
         * At macOS platform, this constant means "Inappropriate ioctl for device"
         * At Windows platform, this constant may have a value `-4029`
         */
        public const ENOTTY = -25;
        /**
         * This constant holds UV_EFTYPE value, it's platform-dependent.
         *
         * At macOS platform, this constant may have a value `-79` means "Inappropriate file type or format"
         */
        public const EFTYPE = -4028;
        /**
         * This constant holds UV_EILSEQ value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Illegal byte sequence"
         * At macOS platform, this constant may have a value `-92` means "Illegal byte sequence"
         * At Windows platform, this constant may have a value `-4027`
         */
        public const EILSEQ = -84;
        /**
         * This constant holds UV_ESOCKTNOSUPPORT value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Socket type not supported"
         * At macOS platform, this constant may have a value `-44` means "Socket type not supported"
         * At Windows platform, this constant may have a value `-4025`
         */
        public const ESOCKTNOSUPPORT = -94;
        /**
         * This constant holds UV_ESTALE value, it's platform-dependent.
         *
         * At Linux platform, this constant means "Stale file handle"
         * At macOS platform, this constant may have a value `-70` means "Stale NFS file handle"
         * At Windows platform, this constant may have a value `-10070`
         */
        public const ESTALE = -116;
        public const UNCODED = -9763;
        public const EPREV = -9762;
        public const EMISUSE = -9761;
        public const EVALUE = -9760;
        public const ELOCKED = -9759;
        public const ECLOSING = -9758;
        public const ECLOSED = -9757;
        public const EDEADLK = -9756;
        public const ESSL = -9755;
        public const ENOCERT = -9754;
        public const ECERT = -9753;
        public const ECHILD = -9752;

        public static function getDescriptionFor(int $error): string { }
    }
}

namespace Swow
{
    class Log
    {
        public const TYPE_DEBUG = 1;
        public const TYPE_INFO = 2;
        public const TYPE_NOTICE = 4;
        public const TYPE_WARNING = 8;
        public const TYPE_ERROR = 16;
        public const TYPE_CORE_ERROR = 32;
        public const TYPES_ALL = 63;
        public const TYPES_DEFAULT = 62;
        public const TYPES_ABNORMAL = 60;
        public const TYPES_UNFILTERABLE = 48;

        public static function getTypes(): int { }

        public static function setTypes(int $types): void { }

        public static function getModuleTypes(): int { }

        public static function setModuleTypes(int $moduleTypes): void { }
    }
}

namespace Swow
{
    class Exception extends \RuntimeException { }
}

namespace Swow
{
    class CallException extends \Swow\Exception
    {
        protected mixed $returnValue;

        final public function getReturnValue(): mixed { }
    }
}

namespace Swow
{
    final class Defer
    {
        public function __construct() { }
    }
}

namespace Swow
{
    class Coroutine
    {
        public const STATE_NONE = 0;
        public const STATE_WAITING = 1;
        public const STATE_RUNNING = 2;
        public const STATE_DEAD = 3;

        public function __construct(callable $callable) { }

        public static function run(callable $callable, mixed ...$data): static { }

        public function resume(mixed ...$data): mixed { }

        public static function yield(mixed $data = null): mixed { }

        public function getId(): int { }

        public static function getCurrent(): self|static { }

        public static function getMain(): self|static { }

        public function getPrevious(): self|static { }

        public function getState(): int { }

        public function getStateName(): string { }

        public function getRound(): int { }

        public static function getCurrentRound(): int { }

        public function getElapsed(): int { }

        public function getElapsedAsString(): string { }

        public function getExitStatus(): int { }

        public function isAvailable(): bool { }

        public function isAlive(): bool { }

        /**
         * get current executed code file name
         *
         * @see Coroutine::getTrace() for params usage
         */
        public function getExecutedFilename(int $level = 0): string { }

        /**
         * get current executed code line
         *
         * @see Coroutine::getTrace() for params usage
         */
        public function getExecutedLineno(int $level = 0): int { }

        /**
         * get executed function name
         *
         * @see Coroutine::getTrace() for params usage
         */
        public function getExecutedFunctionName(int $level = 0): string { }

        /**
         * get trace information
         *
         * positive `$level` value count frame from current to top,
         * negative `$level` value count frame from top to current.
         *
         * for example, a stack may be like:
         *
         * | $level | frame                              | getTrace(0, 0) | getTrace(-1, 0) or getTrace(4, 0) | getTrace(0, 3) |
         * | -      | -                                  | -              | -                                 | -              |
         * | 5      | {main}                             | starts here    | starts here                       | FIXME: main?   |
         * | 4 / -1 | Swow\Coroutine::run('a')           |                | ends here                         |                |
         * | 3 / -2 | a()                                |                |                                   |                |
         * | 2 / -3 | b()                                |                |                                   | starts here    |
         * | 1 / -4 | c()                                |                |                                   |                |
         * | 0 / -5 | Swow\Coroutine->getTraceAsString() | ends here      |                                   | ends here      |
         *
         * @param int $level trace start level
         * @param int $limit trace max length, negative or 0 meaning no limit
         * @param int $options trace options, same as `debug_backtrace` options { @see https://www.php.net/manual/en/function.debug-backtrace.php }
         * @phan-return array<
         * array{
         * 'file': string,
         * 'line': int,
         * 'function': string,
         * 'args': array<mixed>,
         * 'class'?: string
         * }
         * >
         * @phpstan-return array<
         * array{
         * 'file': string,
         * 'line': int,
         * 'function': string,
         * 'args': array<mixed>,
         * 'class'?: string
         * }
         * >
         * @psalm-return array<
         * array{
         * 'file': string,
         * 'line': int,
         * 'function': string,
         * 'args': array<mixed>,
         * 'class'?: string
         * }
         * >
         * @return array<array>
         */
        public function getTrace(int $level = 0, int $limit = 0, int $options = \DEBUG_BACKTRACE_PROVIDE_OBJECT): array { }

        /**
         * get trace information as string like original php strack trace format
         *
         * return value maybe like
         * ```plain
         * #0 some.php(7): Swow\Coroutine->getTraceAsString()
         * #1 some.php(10): c()
         * #2 some.php(13): b()
         * #3 [internal function]: a(1, 'some str', Array)
         * #4 some.php(16): Swow\Coroutine::run('a', 1, 'some str', Array)
         * #5 {main}
         * ```
         *
         * @see Coroutine::getTrace() for params usage
         */
        public function getTraceAsString(int $level = 0, int $limit = 0, int $options = \DEBUG_BACKTRACE_PROVIDE_OBJECT): string { }

        /**
         * get trace information as string like original php strack trace format in array
         *
         * return value maybe like
         * ```php
         * [
         * "#0 some.php(7): Swow\Coroutine->getTraceAsString()",
         * "#1 some.php(10): c()",
         * "#2 some.php(13): b()",
         * "#3 [internal function]: a(1, 'some str', Array)",
         * "#4 some.php(16): Swow\Coroutine::run('a', 1, 'some str', Array)",
         * "#5 {main}",
         * ]
         * ```
         *
         * @return array<string>
         * @see Coroutine::getTrace() for params usage
         */
        public function getTraceAsList(int $level = 0, int $limit = 0, int $options = \DEBUG_BACKTRACE_PROVIDE_OBJECT): array { }

        /**
         * get trace depth
         *
         * @see Coroutine::getTrace() for params usage
         */
        public function getTraceDepth(int $limit = 0): int { }

        /**
         * get defined variables in specified scope
         *
         * @return array<string, mixed>
         * @see Coroutine::getTrace() for params usage
         */
        public function getDefinedVars(int $level = 0): array { }

        /**
         * set variable in specified scope
         *
         * @param string $name variable name
         * @param mixed $value variable value
         * @param bool $force force rebuild symbol table, if you donot know this, keep its default value
         * @see Coroutine::getTrace() for `$level` params usage
         */
        public function setLocalVar(string $name, mixed $value, int $level = 0, bool $force = true): static { }

        /**
         * evaluate code in specified scope
         *
         * @todo string $string? why not rename it to $code
         * @param string $string code to evaluate
         * @see Coroutine::getTrace() for `$level` params usage
         */
        public function eval(string $string, int $level = 0): mixed { }

        public function call(callable $callable, int $level = 0): mixed { }

        public function throw(\Throwable $throwable): mixed { }

        public function kill(): void { }

        public static function killAll(): void { }

        public static function count(): int { }

        public static function get(int $id): ?static { }

        public static function getAll(): array { }

        /** @return array<string, mixed> debug information for var_dump */
        public function __debugInfo(): array { }

        public static function registerDeadlockHandler(callable $callable): Util\Handler { }
    }
}

namespace Swow
{
    class CoroutineException extends \Swow\Exception { }
}

namespace Swow
{
    /**
     * channel transfers information across coroutines
     *
     * @phan-template T
     * @phpstan-template T
     * @psalm-template T
     */
    class Channel
    {
        public const OPCODE_PUSH = 0;
        public const OPCODE_POP = 1;

        public function __construct(int $capacity = 0) { }

        /**
         * push data into channel
         *
         * @note context switching happens here when channel is full, context switching may also happen at this even channel is not full.
         *
         * @phan-param T $data
         * @phpstan-param T $data
         * @psalm-param T $data
         * @param mixed $data
         * @param int $timeout in microseconds
         * @return static
         */
        public function push(mixed $data, int $timeout = -1): static { }

        /**
         * pop data from channel
         *
         * @note context switching happens here when channel is empty, context switching may also happen at this even channel is not empty.
         *
         * @param int $timeout in microseconds
         * @phan-return T $data
         * @phpstan-return T $data
         * @psalm-return T $data
         * @return mixed
         */
        public function pop(int $timeout = -1): mixed { }

        public function close(): void { }

        public function getCapacity(): int { }

        public function getLength(): int { }

        public function isAvailable(): bool { }

        public function hasProducers(): bool { }

        public function hasConsumers(): bool { }

        public function isEmpty(): bool { }

        public function isFull(): bool { }

        public function isReadable(): bool { }

        public function isWritable(): bool { }

        /** @return array<string, mixed> debug information for var_dump */
        public function __debugInfo(): array { }
    }
}

namespace Swow
{
    class ChannelException extends \Swow\Exception { }
}

namespace Swow
{
    class SyncException extends \Swow\Exception { }
}

namespace Swow
{
    class Buffer implements \Stringable
    {
        /**
         * This constant holds page size of this machine, it's platform-dependent.
         *
         * At Linux mips64 and macOS arm64 platforms, this constant may have a value `16384`
         */
        public const PAGE_SIZE = 4096;
        public const DEFAULT_SIZE = 8192;

        public static function alignSize(int $size = 0, int $alignment = 0): int { }

        public function __construct(int $size = self::DEFAULT_SIZE) { }

        public function alloc(int $size = self::DEFAULT_SIZE): static { }

        public function getSize(): int { }

        public function getLength(): int { }

        public function getAvailableSize(): int { }

        public function getReadableLength(): int { }

        public function getWritableSize(): int { }

        public function isReadable(): bool { }

        public function isWritable(): bool { }

        public function isSeekable(): bool { }

        public function isAvailable(): bool { }

        public function isEmpty(): bool { }

        public function isFull(): bool { }

        public function realloc(int $newSize): static { }

        /** @var int $recommendSize [optional] = $this->getSize() * 2 */
        public function extend(?int $recommendSize = null): static { }

        public function mallocTrim(): static { }

        public function tell(): int { }

        public function rewind(): static { }

        public function eof(): bool { }

        /**
         * set the buffer position by `$offset` and `$whence`
         *
         * if `$whence` is
         *
         * - SEEK_SET: sets position to `$offset` from the buffer begin
         * - SEEK_CUR: sets position to `$offset` from the current buffer position, `$offset` can be negative to seek forward
         * - SEEK_END: sets position to `$offset` from the buffer end, `$offset` should be negative or 0
         *
         * @throws BufferException when new buffer position overflow (out of postion 0 to buffer size - 1)
         * @param int $offset offset by `$whence`
         * @param int $whence SEEK_SET or SEEK_CUR or SEEK_END
         */
        public function seek($offset, $whence = \SEEK_SET): static { }

        /**
         * read at max `$length` bytes data from current position, set buffer position to next unread byte
         *
         * @throws \ValueError when specified `$length` not in range [-1, max]
         * @phpstan-param int<-1, max> $length
         * @psalm-param int<-1, max> $length
         * @param int $length -1 meaning read all available, otherwise max length in byte to read
         */
        public function read($length = -1): string { }

        /**
         * read at max `$length` bytes data from current position, not change buffer position
         *
         * @throws \ValueError when specified `$length` not in range [-1, max]
         * @phpstan-param int<-1, max> $length
         * @psalm-param int<-1, max> $length
         * @see Buffer::read() for param usage
         */
        public function peek($length = -1): string { }

        /**
         * read at max `$length` bytes data from specified position, not change buffer position
         *
         * @throws BufferException when specified `$offset` not in range [0, buffer length - 1]
         * @throws \ValueError when specified `$length` not in range [-1, max]
         * @phpstan-param int<0, max>|null $offset
         * @psalm-param int<0, max>|null $offset
         * @param int|null $offset starting position in bytes or null for using {@see Buffer::getOffset()} value
         * @phpstan-param int<-1, max> $length
         * @psalm-param int<-1, max> $length
         * @param int $length -1 meaning read all available, otherwise max length in byte to read
         */
        public function peekFrom(?int $offset = null, int $length = -1): string { }

        public function getContents(): string { }

        /**
         * write `$string` to buffer, set buffer position to the end of written string
         *
         * `$offset` and `$length` is for input `$string`
         *
         * @throws \ValueError when specified `$offset` not in range [0, string length - 1]
         * @throws \ValueError when specified `$length` not in range [-1, available string length]
         * @param string $string data to write
         * @phpstan-param int<0, max> $offset
         * @psalm-param int<0, max> $offset
         * @param int $offset starting offset in bytes
         * @phpstan-param int<-1, max> $length
         * @psalm-param int<-1, max> $length
         * @param int $length max length in bytes
         */
        public function write($string, int $offset = 0, int $length = -1): static { }

        /**
         * write `$string` to buffer, not change buffer position
         *
         * `$offset` and `$length` is for input `$string`
         *
         * @throws \ValueError when specified `$offset` not in range [0, string length - 1]
         * @throws \ValueError when specified `$length` not in range [-1, available string length]
         * @param string $string data to write
         * @phpstan-param int<0, max> $offset
         * @psalm-param int<0, max> $offset
         * @param int $offset starting offset in bytes
         * @phpstan-param int<-1, max> $length
         * @psalm-param int<-1, max> $length
         * @param int $length max length in bytes
         */
        public function copy(string $string, int $offset = 0, int $length = -1): static { }

        public function truncate(int $length = -1): static { }

        /** @var int $offset [optional] = $this->getOffset() */
        public function truncateFrom(?int $offset = null, int $length = -1): static { }

        public function clear(): static { }

        public function fetchString(): string { }

        public function dupString(): string { }

        public function toString(): string { }

        public function lock(): void { }

        public function tryLock(): bool { }

        public function unlock(): void { }

        public function close(): void { }

        public function __toString(): string { }

        /** @return array<string, mixed> debug information for var_dump */
        public function __debugInfo(): array { }
    }
}

namespace Swow
{
    class BufferException extends \Swow\Exception { }
}

namespace Swow
{
    class Socket
    {
        public const INVALID_FD = -1;
        public const DEFAULT_BACKLOG = 511;
        public const TYPE_FLAG_STREAM = 1;
        public const TYPE_FLAG_DGRAM = 2;
        public const TYPE_FLAG_INET = 16;
        public const TYPE_FLAG_IPV4 = 32;
        public const TYPE_FLAG_IPV6 = 64;
        public const TYPE_FLAG_LOCAL = 128;
        public const TYPE_FLAG_IPC = 1024;
        public const TYPE_FLAG_STDIN = 1024;
        public const TYPE_FLAG_STDOUT = 2048;
        public const TYPE_FLAG_STDERR = 4096;
        public const TYPE_ANY = 0;
        public const TYPE_TCP = 16777233;
        public const TYPE_TCP4 = 16777265;
        public const TYPE_TCP6 = 16777297;
        public const TYPE_PIPE = 33554561;
        public const TYPE_IPCC = 33555585;
        public const TYPE_TTY = 67108865;
        public const TYPE_STDIN = 67109889;
        public const TYPE_STDOUT = 67110913;
        public const TYPE_STDERR = 67112961;
        public const TYPE_UDP = 134217746;
        public const TYPE_UDP4 = 134217778;
        public const TYPE_UDP6 = 134217810;
        /** UNIX domain socket type, this constant is only available at unix-like os. */
        public const TYPE_UNIX = 33554561;
        /** UNIX datagram socket type, this constant is only avaliable at unix-like os. */
        public const TYPE_UDG = 268435586;
        public const IO_FLAG_NONE = 0;
        public const IO_FLAG_READ = 1;
        public const IO_FLAG_WRITE = 2;
        public const IO_FLAG_RDWR = 3;
        public const IO_FLAG_BIND = 7;
        public const IO_FLAG_ACCEPT = 11;
        public const IO_FLAG_CONNECT = 19;
        public const BIND_FLAG_NONE = 0;
        public const BIND_FLAG_IPV6ONLY = 1;
        public const BIND_FLAG_REUSEADDR = 2;
        public const BIND_FLAG_REUSEPORT = 4;

        public function __construct(int $type = self::TYPE_TCP) { }

        public function getId(): int { }

        public function getType(): int { }

        public function getSimpleType(): int { }

        public function getTypeName(): string { }

        public function getSimpleTypeName(): string { }

        public function getFd(): int { }

        public function getDnsTimeout(): int { }

        public function getAcceptTimeout(): int { }

        public function getConnectTimeout(): int { }

        public function getHandshakeTimeout(): int { }

        public function getReadTimeout(): int { }

        public function getWriteTimeout(): int { }

        public function setTimeout(int $timeout): static { }

        public function setDnsTimeout(int $timeout): static { }

        public function setAcceptTimeout(int $timeout): static { }

        public function setConnectTimeout(int $timeout): static { }

        public function setHandshakeTimeout(int $timeout): static { }

        public function setReadTimeout(int $timeout): static { }

        public function setWriteTimeout(int $timeout): static { }

        public function bind(string $name, int $port = 0, int $flags = self::BIND_FLAG_NONE): static { }

        public function listen(int $backlog = self::DEFAULT_BACKLOG): static { }

        /**
         * @var int $timeout [optional] = $this->getAcceptTimeout()
         * @retrun static Notice: it returns a new un-constructed connection instead of returning itself
         */
        public function accept(?int $timeout = null): static { }

        /**
         * @var int $timeout [optional] = $this->getAcceptTimeout()
         * @retrun $this Notice: it does not return the connection, but returns itself
         */
        public function acceptTo(self $connection, ?int $timeout = null): static { }

        /** @var int $timeout [optional] = $this->getConnectTimeout() */
        public function connect(string $name, int $port = 0, ?int $timeout = null): static { }

        public function getSockAddress(): string { }

        public function getSockPort(): int { }

        public function getPeerAddress(): string { }

        public function getPeerPort(): int { }

        /**
         * read `$length` bytes data into buffer from socket,
         * keep reading until `$length` bytes data received.
         *
         * @note context switching may happen here
         *
         * @throws SocketException when got eof without enough data received
         * @throws SocketException when timed out
         * @throws SocketException when socket read failed
         * @param Buffer $buffer buffer to write in, data will be written from buffer current position
         * @phan-param int<-1, max> $length
         * @psalm-param int<-1, max> $length
         * @param int $length -1 meaning not limited, receive until eof, otherwise length in bytes
         * @param int|null $timeout timeout in microseconds or null for using {@see Socket::getReadTimeout()} value
         * @return int bytes received
         */
        public function read(\Swow\Buffer $buffer, int $length = -1, ?int $timeout = null): int { }

        /**
         * read at max `$size` bytes data into buffer from socket,
         * returns when any data received or eof.
         *
         * @note context switching may happen here
         *
         * @throws SocketException when timed out
         * @throws SocketException when socket read failed
         * @param Buffer $buffer buffer to write in, data will be written from buffer current position
         * @phan-param int<-1, max> $size
         * @psalm-param int<-1, max> $size
         * @param int $size -1 meaning not limited, otherwise buffer size in bytes. only `$size` bytes data will be read from socket if there are more data than `$size` bytes, extra data will be kept in socket for further reading options
         * @param int|null $timeout timeout in microseconds or null for using {@see Socket::getReadTimeout()} value
         * @return int bytes received
         */
        public function recv(\Swow\Buffer $buffer, int $size = -1, ?int $timeout = null): int { }

        /**
         * read at max `$size` bytes data into buffer from socket,
         * return when any data received.
         *
         * @note context switching may happen here
         *
         * @throws SocketException when no data received
         * @throws SocketException when timed out
         * @throws SocketException when socket read failed
         * @param Buffer $buffer buffer to write in, data will be written from buffer current position
         * @phan-param int<-1, max> $size
         * @psalm-param int<-1, max> $size
         * @param int $size -1 meaning not limited, otherwise buffer size in bytes. only `$size` bytes data will be read from socket if there are more data than `$size` bytes, extra data will be kept in socket for further reading options
         * @param int|null $timeout timeout in microseconds or null for using {@see Socket::getReadTimeout()} value
         */
        public function recvData(\Swow\Buffer $buffer, int $size = -1, ?int $timeout = null): int { }

        /**
         * read at max `$size` bytes data into buffer from socket,
         * peer info is stored into `$address` and `$port` if applicable,
         * return when any data received or eof.
         *
         * @note context switching may happen here
         *
         * @throws SocketException when timed out
         * @throws SocketException when socket read failed
         * @param Buffer $buffer buffer to write in, data will be written from buffer current position
         * @phan-param int<-1, max> $size
         * @psalm-param int<-1, max> $size
         * @param int $size -1 meaning not limited, otherwise buffer size in bytes. only `$size` bytes data will be read from socket if there are more data than `$size` bytes, extra data will be kept in socket for further reading options
         * @param string &$address peer address in string
         * @phpstan-param int<0,65535>|null &$address
         * @psalm-param int<0,65535>|null &$address
         * @param int &$port peer address
         * @param int|null $timeout timeout in microseconds or null for using {@see Socket::getReadTimeout()} value
         * @return int bytes received
         */
        public function recvFrom(\Swow\Buffer $buffer, int $size = -1, &$address = null, &$port = null, ?int $timeout = null): int { }

        /**
         * read at max `$size` bytes data into buffer from socket,
         * peer info is stored into `$address` and `$port` if applicable,
         * return when any data received.
         *
         * @note context switching may happen here
         *
         * @throws SocketException when no data received
         * @throws SocketException when timed out
         * @throws SocketException when socket read failed
         * @param Buffer $buffer buffer to write in, data will be written from buffer current position
         * @phan-param int<-1, max> $size
         * @psalm-param int<-1, max> $size
         * @param int $size -1 meaning not limited, otherwise buffer size in bytes. only `$size` bytes data will be read from socket if there are more data than `$size` bytes, extra data will be kept in socket for further reading options
         * @param string &$address peer address in string
         * @phpstan-param int<0,65535>|null &$address
         * @psalm-param int<0,65535>|null &$address
         * @param int &$port peer address
         * @param int|null $timeout timeout in microseconds or null for using {@see Socket::getReadTimeout()} value
         * @return int bytes received
         */
        public function recvDataFrom(\Swow\Buffer $buffer, int $size = -1, &$address = null, &$port = null, ?int $timeout = null): int { }

        /**
         * read at max `$size` bytes data into buffer from socket without removing the read data from socket,
         * only works on some type of socket,
         * returns immediately.
         *
         * @throws SocketException when socket read failed
         * @param Buffer $buffer buffer to write in, data will be written from buffer current position
         * @phan-param int<-1, max> $size
         * @psalm-param int<-1, max> $size
         * @param int $size -1 meaning not limited, otherwise buffer size in bytes.
         * @return int bytes received
         */
        public function peek(\Swow\Buffer $buffer, int $size = -1): int { }

        /**
         * read at max `$size` bytes data into buffer from socket without removing the read data from socket,
         * only works on some type of socket,
         * peer info is stored into `$address` and `$port` if applicable,
         * returns immediately.
         *
         * @throws SocketException when socket read failed
         * @param Buffer $buffer buffer to write in, data will be written from buffer current position
         * @phan-param int<-1, max> $size
         * @psalm-param int<-1, max> $size
         * @param int $size -1 meaning not limited, otherwise buffer size in bytes.
         * @param string &$address peer address in string
         * @phpstan-param int<0,65535>|null &$address
         * @psalm-param int<0,65535>|null &$address
         * @param int &$port peer address
         * @return int bytes received
         */
        public function peekFrom(\Swow\Buffer $buffer, int $size = -1, &$address = null, &$port = null): int { }

        /**
         * read `$length` bytes data from socket as string,
         * keep reading until `$length` bytes data received.
         *
         * @note context switching may happen here
         *
         * @throws SocketException when got eof without enough data received
         * @throws SocketException when timed out
         * @throws SocketException when socket read failed
         * @phan-param int<-1, max> $length
         * @psalm-param int<-1, max> $length
         * @param int $length -1 meaning not limited, receive until eof, otherwise length in bytes
         * @param int|null $timeout timeout in microseconds or null for using {@see Socket::getReadTimeout()} value
         * @return string data received in string
         */
        public function readString(int $length = \Swow\Buffer::DEFAULT_SIZE, ?int $timeout = null): string { }

        /**
         * read at max `$size` bytes data from socket as string,
         * returns when any data received or eof.
         *
         * @note context switching may happen here
         *
         * @throws SocketException when timed out
         * @throws SocketException when socket read failed
         * @phan-param int<-1, max> $size
         * @psalm-param int<-1, max> $size
         * @param int $size -1 meaning not limited, otherwise buffer size in bytes. only `$size` bytes data will be read from socket if there are more data than `$size` bytes, extra data will be kept in socket for further reading options
         * @param int|null $timeout timeout in microseconds or null for using {@see Socket::getReadTimeout()} value
         * @return string data received in string
         */
        public function recvString(int $size = \Swow\Buffer::DEFAULT_SIZE, ?int $timeout = null): string { }

        /**
         * read at max `$size` bytes data from socket as string,
         * return when any data received.
         *
         * @note context switching may happen here
         *
         * @throws SocketException when no data received
         * @throws SocketException when timed out
         * @throws SocketException when socket read failed
         * @phan-param int<-1, max> $size
         * @psalm-param int<-1, max> $size
         * @param int $size -1 meaning not limited, otherwise buffer size in bytes. only `$size` bytes data will be read from socket if there are more data than `$size` bytes, extra data will be kept in socket for further reading options
         * @param int|null $timeout timeout in microseconds or null for using {@see Socket::getReadTimeout()} value
         * @return string data received in string
         */
        public function recvStringData(int $size = \Swow\Buffer::DEFAULT_SIZE, ?int $timeout = null): string { }

        /**
         * read at max `$size` bytes data from socket as string,
         * peer info is stored into `$address` and `$port` if applicable,
         * return when any data received or eof.
         *
         * @note context switching may happen here
         *
         * @throws SocketException when timed out
         * @throws SocketException when socket read faile
         * @phan-param int<-1, max> $size
         * @psalm-param int<-1, max> $size
         * @param int $size -1 meaning not limited, otherwise buffer size in bytes. only `$size` bytes data will be read from socket if there are more data than `$size` bytes, extra data will be kept in socket for further reading options
         * @param string &$address peer address in string
         * @phpstan-param int<0,65535>|null &$address
         * @psalm-param int<0,65535>|null &$address
         * @param int &$port peer address
         * @param int|null $timeout timeout in microseconds or null for using {@see Socket::getReadTimeout()} value
         * @return string data received in string
         */
        public function recvStringFrom(int $size = \Swow\Buffer::DEFAULT_SIZE, &$address = null, &$port = null, ?int $timeout = null): string { }

        /**
         * read at max `$size` bytes data from socket as string,
         * peer info is stored into `$address` and `$port` if applicable,
         * return when any data received.
         *
         * @note context switching may happen here
         *
         * @throws SocketException when no data received
         * @throws SocketException when timed out
         * @throws SocketException when socket read failed
         * @phan-param int<-1, max> $size
         * @psalm-param int<-1, max> $size
         * @param int $size -1 meaning not limited, otherwise buffer size in bytes. only `$size` bytes data will be read from socket if there are more data than `$size` bytes, extra data will be kept in socket for further reading options
         * @param string &$address peer address in string
         * @phpstan-param int<0,65535>|null &$address
         * @psalm-param int<0,65535>|null &$address
         * @param int &$port peer address
         * @param int|null $timeout timeout in microseconds or null for using {@see Socket::getReadTimeout()} value
         * @return string data received in string
         */
        public function recvStringDataFrom(int $size = \Swow\Buffer::DEFAULT_SIZE, &$address = null, &$port = null, ?int $timeout = null): string { }

        /**
         * read at max `$size` bytes data from socket as string without removing the read data from socket,
         * only works on some type of socket,
         * returns immediately.
         *
         * @throws SocketException when socket read failed
         * @phan-param int<-1, max> $size
         * @psalm-param int<-1, max> $size
         * @param int $size -1 meaning not limited, otherwise buffer size in bytes.
         * @return string data received in string
         */
        public function peekString(int $size = \Swow\Buffer::DEFAULT_SIZE): string { }

        /**
         * read at max `$size` bytes data from socket as string without removing the read data from socket,
         * only works on some type of socket,
         * peer info is stored into `$address` and `$port` if applicable,
         * returns immediately.
         *
         * @throws SocketException when socket read failed
         * @phan-param int<-1, max> $size
         * @psalm-param int<-1, max> $size
         * @param int $size -1 meaning not limited, otherwise buffer size in bytes.
         * @param string &$address peer address in string
         * @phpstan-param int<0,65535>|null &$address
         * @psalm-param int<0,65535>|null &$address
         * @param int &$port peer address
         * @return string data received in string
         */
        public function peekStringFrom(int $size = \Swow\Buffer::DEFAULT_SIZE, &$address = null, &$port = null): string { }

        /**
         * write io vector to socket
         *
         * vector is an array for all contents to write, elements in it maybe
         * - `Buffer`: write full text from current position
         * - `string|Stringable`: write full text from start
         * - `array[Buffer $content, int $length]`: write it from current position, with length `$length`
         * - `array[string|Stringable $content, int $offset, ?int $length]`: write the string from index `$offset`, with length `$length`
         *
         * for code
         * ```php
         * $s = new Socket(Socket::TYPE_STDOUT);
         * $s->write([
         * (new Buffer())->write('1234567890')->seek(6),
         * 'abc',
         * ['1234567890', 1, 2],
         * [(new Buffer())->write('1234567890')->seek(4), 2],
         * ]);
         * ```
         * string `"7890abc2356"` will be written to the socket.
         *
         * @note context switching may happen here
         *
         * @throws SocketException when timed out
         * @throws SocketException when write failed
         * @phan-param non-empty-array<string|\Stringable|Buffer|array{0: string|\Stringable, 1: int, 2?: int}|array{0: Buffer, 1: int}|null> $vector
         * @phpstan-param non-empty-array<string|\Stringable|Buffer|array{0: string|\Stringable, 1: int, 2?: int}|array{0: Buffer, 1: int}|null> $vector
         * @psalm-param non-empty-array<string|\Stringable|Buffer|array{0: string|\Stringable, 1: int, 2?: int}|array{0: Buffer, 1: int}|null> $vector
         * @param array<string|\Stringable|Buffer|array|null> $vector
         * @param int|null $timeout timeout in microseconds or null for using {@see Socket::getWriteTimeout()} value
         */
        public function write(array $vector, ?int $timeout = null): static { }

        /**
         * write io vector to socket with peer information
         *
         * @see Socket::write() for io vector format
         *
         * @note context switching may happen here
         *
         * @throws SocketException when timed out
         * @throws SocketException when write failed
         * @phan-param non-empty-array<string|\Stringable|Buffer|array{0: string|\Stringable, 1: int, 2?: int}|array{0: Buffer, 1: int}|null> $vector
         * @phpstan-param non-empty-array<string|\Stringable|Buffer|array{0: string|\Stringable, 1: int, 2?: int}|array{0: Buffer, 1: int}|null> $vector
         * @psalm-param non-empty-array<string|\Stringable|Buffer|array{0: string|\Stringable, 1: int, 2?: int}|array{0: Buffer, 1: int}|null> $vector
         * @param array<string|\Stringable|Buffer|array|null> $vector
         * @param string|null $address address to send to, may be ip or domain or path (for UNIX/UDG type)
         * @phpstan-param int<0, 65535>|null $port
         * @psalm-param int<0, 65535>|null $port
         * @param int|null $port port to send to
         * @param int|null $timeout timeout in microseconds or null for using {@see Socket::getWriteTimeout()} value
         */
        public function writeTo(array $vector, ?string $address = null, ?int $port = null, ?int $timeout = null): static { }

        /**
         * write buffer into socket from buffer current position with `$length` bytes
         *
         * @throws SocketException when timed out
         * @throws SocketException when write failed
         * @phpstan-param int<-1, max> $length
         * @psalm-param int<-1, max> $length
         * @param int $length length of data to be sent, -1 meaning remaining data in buffer, otherwise length in bytes.
         * @param int|null $timeout timeout in microseconds or null for using {@see Socket::getWriteTimeout()} value
         */
        public function send(\Swow\Buffer $buffer, int $length = -1, ?int $timeout = null): static { }

        /**
         * write buffer to specified address (and port if protocol needs port) from buffer current position with `$length` bytes
         *
         * @throws SocketException when timed out
         * @throws SocketException when write failed
         * @phpstan-param int<-1, max> $length
         * @psalm-param int<-1, max> $length
         * @param int $length length of data to be sent, -1 meaning remaining data in buffer, otherwise length in bytes.
         * @param int|null $timeout timeout in microseconds or null for using {@see Socket::getWriteTimeout()} value
         * @param string|null $address address to send to, may be ip or domain or path (for UNIX/UDG type)
         * @phpstan-param int<0, 65535>|null $port
         * @psalm-param int<0, 65535>|null $port
         * @param int|null $port port to send to
         */
        public function sendTo(\Swow\Buffer $buffer, int $length = -1, ?string $address = null, ?int $port = null, ?int $timeout = null): static { }

        /**
         * write `$length` bytes of string into socket from `$offset` byte
         *
         * @throws SocketException when timed out
         * @throws SocketException when write failed
         * @param int|null $timeout timeout in microseconds or null for using {@see Socket::getWriteTimeout()} value
         * @phpstan-param int<0, max> $offset
         * @psalm-param int<0, max> $offset
         * @param int $offset indexed string starts from 0: `sendString('1234567890', offset: 2, length: 2)` will write `'34'` into socket
         * @phpstan-param int<-1, max> $length
         * @psalm-param int<-1, max> $length
         * @param int $length length of data to be sent, -1 meaning remaining data, otherwise length in bytes.
         */
        public function sendString(string $string, ?int $timeout = null, int $offset = 0, int $length = -1): static { }

        /**
         * write `$length` bytes string to specified address (and port if protocol needs port) from `$offset` byte
         *
         * @throws SocketException when timed out
         * @throws SocketException when write failed
         * @param string|null $address address to send to, may be ip or domain or path (for UNIX/UDG type)
         * @phpstan-param int<0, 65535>|null $port
         * @psalm-param int<0, 65535>|null $port
         * @param int|null $port port to send to
         * @param int|null $timeout timeout in microseconds or null for using {@see Socket::getWriteTimeout()} value
         * @phpstan-param int<0, max> $offset
         * @psalm-param int<0, max> $offset
         * @param int $offset indexed string starts from 0: `sendStringTo('1234567890', offset: 2, length: 2, ...)` will write `'34'` into socket
         * @phpstan-param int<-1, max> $length
         * @psalm-param int<-1, max> $length
         * @param int $length length of data to be sent, -1 meaning remaining data, otherwise length in bytes.
         */
        public function sendStringTo(string $string, ?string $address = null, ?int $port = null, ?int $timeout = null, int $offset = 0, int $length = -1): static { }

        /** @var int $timeout [optional] = $this->getWriteTimeout() */
        public function sendHandle(self $handle, ?int $timeout = null): static { }

        public function close(): bool { }

        public function isAvailable(): bool { }

        public function isOpen(): bool { }

        public function isEstablished(): bool { }

        public function isServer(): bool { }

        public function isServerConnection(): bool { }

        public function isClient(): bool { }

        public function getConnectionError(): int { }

        public function checkLiveness(): static { }

        public function getIoState(): int { }

        public function getIoStateName(): string { }

        public function getIoStateNaming(): string { }

        public function getRecvBufferSize(): int { }

        public function getSendBufferSize(): int { }

        public function setRecvBufferSize(int $size): static { }

        public function setSendBufferSize(int $size): static { }

        public function setTcpNodelay(bool $enable): static { }

        public function setTcpKeepAlive(bool $enable, int $delay): static { }

        public function setTcpAcceptBalance(bool $enable): static { }

        /** @return array<string, mixed> debug information for var_dump */
        public function __debugInfo(): array { }

        public static function typeSimplify(int $type): int { }

        public static function typeName(int $type): string { }

        public static function setGlobalTimeout(int $timeout): void { }

        public static function getGlobalDnsTimeout(): int { }

        public static function getGlobalAcceptTimeout(): int { }

        public static function getGlobalConnectTimeout(): int { }

        public static function getGlobalHandshakeTimeout(): int { }

        public static function getGlobalReadTimeout(): int { }

        public static function getGlobalWriteTimeout(): int { }

        public static function setGlobalDnsTimeout(int $timeout): void { }

        public static function setGlobalAcceptTimeout(int $timeout): void { }

        public static function setGlobalConnectTimeout(int $timeout): void { }

        public static function setGlobalHandshakeTimeout(int $timeout): void { }

        public static function setGlobalReadTimeout(int $timeout): void { }

        public static function setGlobalWriteTimeout(int $timeout): void { }
    }
}

namespace Swow
{
    class SocketException extends \Swow\CallException { }
}

namespace Swow
{
    class Signal
    {
        /**
         * This constant holds SIGHUP value, it's platform-dependent.
         *
         * At macOS platform, this constant means "hangup"
         * At Windows platform, this constant may not exist
         */
        public const HUP = 1;
        /**
         * This constant holds SIGINT value, it's platform-dependent.
         *
         * At macOS and Windows platforms, this constant means "interrupt"
         */
        public const INT = 2;
        /**
         * This constant holds SIGQUIT value, it's platform-dependent.
         *
         * At macOS platform, this constant means "quit"
         * At Windows platform, this constant may not exist
         */
        public const QUIT = 3;
        /**
         * This constant holds SIGILL value, it's platform-dependent.
         *
         * At macOS platform, this constant means "illegal instruction (not reset when caught)"
         * At Windows platform, this constant means "illegal instruction - invalid function image"
         */
        public const ILL = 4;
        /**
         * This constant holds SIGTRAP value, it's platform-dependent.
         *
         * At macOS platform, this constant means "trace trap (not reset when caught)"
         * At Windows platform, this constant may not exist
         */
        public const TRAP = 5;
        /**
         * This constant holds SIGABRT value, it's platform-dependent.
         *
         * At macOS platform, this constant means "abort()"
         * At Windows platform, this constant may have a value `22` means "abnormal termination triggered by abort call"
         */
        public const ABRT = 6;
        /**
         * This constant holds SIGIOT value, it's platform-dependent.
         *
         * At macOS and Windows platforms, this constant may not exist
         */
        public const IOT = 6;
        /**
         * This constant holds SIGBUS value, it's platform-dependent.
         *
         * At Linux mips64 platform, this constant may have a value `10`
         * At macOS platform, this constant may have a value `10` means "bus error"
         * At Windows platform, this constant may not exist
         */
        public const BUS = 7;
        /**
         * This constant holds SIGEMT value, it's platform-dependent.
         *
         * At macOS platform, this constant means "EMT instruction"
         * At Windows, Linux x86_64, Linux arm64 and Linux riscv64 platforms, this constant may not exist
         */
        public const EMT = 7;
        /**
         * This constant holds SIGPOLL value, it's platform-dependent.
         *
         * At macOS platform, this constant means "pollable event ([XSR] generated, not supported)"
         * At Linux and Windows platforms, this constant may not exist
         */
        public const POLL = 7;
        /**
         * This constant holds SIGFPE value, it's platform-dependent.
         *
         * At macOS and Windows platforms, this constant means "floating point exception"
         */
        public const FPE = 8;
        /**
         * This constant holds SIGKILL value, it's platform-dependent.
         *
         * At macOS platform, this constant means "kill (cannot be caught or ignored)"
         * At Windows platform, this constant may not exist
         */
        public const KILL = 9;
        /**
         * This constant holds SIGUSR1 value, it's platform-dependent.
         *
         * At Linux mips64 platform, this constant may have a value `16`
         * At macOS platform, this constant may have a value `30` means "user defined signal 1"
         * At Windows platform, this constant may not exist
         */
        public const USR1 = 10;
        /**
         * This constant holds SIGSEGV value, it's platform-dependent.
         *
         * At macOS platform, this constant means "segmentation violation"
         * At Windows platform, this constant means "segment violation"
         */
        public const SEGV = 11;
        /**
         * This constant holds SIGUSR2 value, it's platform-dependent.
         *
         * At Linux mips64 platform, this constant may have a value `17`
         * At macOS platform, this constant may have a value `31` means "user defined signal 2"
         * At Windows platform, this constant may not exist
         */
        public const USR2 = 12;
        /**
         * This constant holds SIGPIPE value, it's platform-dependent.
         *
         * At macOS platform, this constant means "write on a pipe with no one to read it"
         * At Windows platform, this constant may not exist
         */
        public const PIPE = 13;
        /**
         * This constant holds SIGALRM value, it's platform-dependent.
         *
         * At macOS platform, this constant means "alarm clock"
         * At Windows platform, this constant may not exist
         */
        public const ALRM = 14;
        /**
         * This constant holds SIGTERM value, it's platform-dependent.
         *
         * At macOS platform, this constant means "software termination signal from kill"
         * At Windows platform, this constant means "Software termination signal from kill"
         */
        public const TERM = 15;
        /**
         * This constant holds SIGSTKFLT value, it's platform-dependent.
         *
         * At macOS and Windows platforms, this constant may not exist
         */
        public const STKFLT = 16;
        /**
         * This constant holds SIGCHLD value, it's platform-dependent.
         *
         * At Linux mips64 platform, this constant may have a value `18`
         * At macOS platform, this constant may have a value `20` means "to parent on child stop or exit"
         * At Windows platform, this constant may not exist
         */
        public const CHLD = 17;
        /**
         * This constant holds SIGCONT value, it's platform-dependent.
         *
         * At Linux mips64 platform, this constant may have a value `25`
         * At macOS platform, this constant may have a value `19` means "continue a stopped process"
         * At Windows platform, this constant may not exist
         */
        public const CONT = 18;
        /**
         * This constant holds SIGSTOP value, it's platform-dependent.
         *
         * At Linux mips64 platform, this constant may have a value `23`
         * At macOS platform, this constant may have a value `17` means "sendable stop signal not from tty"
         * At Windows platform, this constant may not exist
         */
        public const STOP = 19;
        /**
         * This constant holds SIGTSTP value, it's platform-dependent.
         *
         * At Linux mips64 platform, this constant may have a value `24`
         * At macOS platform, this constant may have a value `18` means "stop signal from tty"
         * At Windows platform, this constant may not exist
         */
        public const TSTP = 20;
        /**
         * This constant holds SIGBREAK value, it's platform-dependent.
         *
         * At Windows platform, this constant means "Ctrl-Break sequence"
         * At Linux and macOS platforms, this constant may not exist
         */
        public const BREAK = 21;
        /**
         * This constant holds SIGTTIN value, it's platform-dependent.
         *
         * At Linux mips64 platform, this constant may have a value `26`
         * At macOS platform, this constant means "to readers pgrp upon background tty read"
         * At Windows platform, this constant may not exist
         */
        public const TTIN = 21;
        /**
         * This constant holds SIGTTOU value, it's platform-dependent.
         *
         * At Linux mips64 platform, this constant may have a value `27`
         * At macOS platform, this constant means "like TTIN for output if (tp->t_local&LTOSTOP)"
         * At Windows platform, this constant may not exist
         */
        public const TTOU = 22;
        /**
         * This constant holds SIGURG value, it's platform-dependent.
         *
         * At Linux mips64 platform, this constant may have a value `21`
         * At macOS platform, this constant may have a value `16` means "urgent condition on IO channel"
         * At Windows platform, this constant may not exist
         */
        public const URG = 23;
        /**
         * This constant holds SIGXCPU value, it's platform-dependent.
         *
         * At Linux mips64 platform, this constant may have a value `30`
         * At macOS platform, this constant means "exceeded CPU time limit"
         * At Windows platform, this constant may not exist
         */
        public const XCPU = 24;
        /**
         * This constant holds SIGXFSZ value, it's platform-dependent.
         *
         * At Linux mips64 platform, this constant may have a value `31`
         * At macOS platform, this constant means "exceeded file size limit"
         * At Windows platform, this constant may not exist
         */
        public const XFSZ = 25;
        /**
         * This constant holds SIGVTALRM value, it's platform-dependent.
         *
         * At Linux mips64 platform, this constant may have a value `28`
         * At macOS platform, this constant means "virtual time alarm"
         * At Windows platform, this constant may not exist
         */
        public const VTALRM = 26;
        /**
         * This constant holds SIGPROF value, it's platform-dependent.
         *
         * At Linux mips64 platform, this constant may have a value `29`
         * At macOS platform, this constant means "profiling time alarm"
         * At Windows platform, this constant may not exist
         */
        public const PROF = 27;
        /**
         * This constant holds SIGWINCH value, it's platform-dependent.
         *
         * At Linux mips64 platform, this constant may have a value `20`
         * At macOS platform, this constant means "window size changes"
         * At Windows platform, this constant may not exist
         */
        public const WINCH = 28;
        /**
         * This constant holds SIGINFO value, it's platform-dependent.
         *
         * At macOS platform, this constant means "information request"
         * At Linux and Windows platforms, this constant may not exist
         */
        public const INFO = 29;
        /**
         * This constant holds SIGIO value, it's platform-dependent.
         *
         * At Linux mips64 platform, this constant may have a value `22`
         * At macOS platform, this constant may have a value `23` means "input/output possible signal"
         * At Windows platform, this constant may not exist
         */
        public const IO = 29;
        /**
         * This constant holds SIGLOST value, it's platform-dependent.
         *
         * At macOS and Windows platforms, this constant may not exist
         */
        public const LOST = 29;
        /**
         * This constant holds SIGPWR value, it's platform-dependent.
         *
         * At Linux mips64 platform, this constant may have a value `19`
         * At macOS and Windows platforms, this constant may not exist
         */
        public const PWR = 30;
        /**
         * This constant holds SIGSYS value, it's platform-dependent.
         *
         * At Linux mips64 platform, this constant may have a value `12`
         * At macOS platform, this constant may have a value `12` means "bad argument to system call"
         * At Windows platform, this constant may not exist
         */
        public const SYS = 31;

        public static function wait(int $num, int $timeout = -1): void { }

        public static function kill(int $pid, int $signum): void { }
    }
}

namespace Swow
{
    class SignalException extends \Swow\Exception { }
}

namespace Swow
{
    class WatchDog
    {
        public static function run(int $quantum = 0, int $threshold = 0, callable|int|float|null $alerter = null): void { }

        public static function stop(): void { }

        public static function isRunning(): bool { }
    }
}

namespace Swow
{
    class WatchDogException extends \Swow\Exception { }
}

namespace Swow
{
    class WebSocket
    {
        public const VERSION = 13;
        public const SECRET_KEY_LENGTH = 16;
        public const SECRET_KEY_ENCODED_LENGTH = 24;
        public const GUID = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11';
        public const HEADER_LENGTH = 2;
        public const EXT16_LENGTH = 126;
        public const EXT64_LENGTH = 127;
        public const EXT8_MAX_LENGTH = 125;
        public const EXT16_MAX_LENGTH = 65535;
        public const MASK_KEY_LENGTH = 4;
        public const EMPTY_MASK_KEY = '';
        public const HEADER_BUFFER_SIZE = 128;
    }
}

namespace Swow
{
    function defer(callable $tasks): void { }
}

namespace Swow\Util
{
    class Handler
    {
        protected function __construct() { }

        public function remove(): void { }
    }
}

namespace Swow\Channel
{
    /**
     * channel selector selects any action done
     * which action can be "pushing/poping `T` into/from (may be different) channel"
     *
     * @phan-template T
     * @phpstan-template T
     * @psalm-template T
     */
    class Selector
    {
        /**
         * add a "pushing a `T` into `$channel`" action into selector
         *
         * @phan-param \Swow\Channel<T> $channel
         * @phpstan-param \Swow\Channel<T> $channel
         * @psalm-param \Swow\Channel<T> $channel
         * @phan-param T $data
         * @phpstan-param T $data
         * @psalm-param T $data
         */
        public function push(\Swow\Channel $channel, mixed $data): static { }

        /**
         * add a "popping a `T` from `$channel`" action into selector
         *
         * @phan-param \Swow\Channel<T> $channel
         * @phpstan-param \Swow\Channel<T> $channel
         * @psalm-param \Swow\Channel<T> $channel
         */
        public function pop(\Swow\Channel $channel): static { }

        /**
         * do select
         *
         * any actions in selector will make this return
         *
         * @note content switching may happen here
         *
         * @param int $timeout timeout in microseconds
         * @phan-return \Swow\Channel<T>
         * @phpstan-return \Swow\Channel<T>
         * @psalm-return \Swow\Channel<T>
         */
        public function commit(int $timeout = -1): \Swow\Channel { }

        public function fetch(): mixed { }

        public function getLastOpcode(): int { }
    }
}

namespace Swow\Channel
{
    class SelectorException extends \Swow\CallException { }
}

namespace Swow\Sync
{
    class WaitReference
    {
        public static function wait(self &$waitReference, int $timeout = -1): void { }

        public function __destruct() { }
    }
}

namespace Swow\Sync
{
    class WaitGroup
    {
        public function add(int $delta = 1): void { }

        public function wait(int $timeout = -1): void { }

        public function done(): void { }
    }
}

namespace Swow\Sync
{
    function waitAll(int $timeout = -1): void { }
}

namespace Swow\Http
{
    class Status
    {
        public const CONTINUE = 100;
        public const SWITCHING_PROTOCOLS = 101;
        public const PROCESSING = 102;
        public const EARLY_HINTS = 103;
        public const OK = 200;
        public const CREATED = 201;
        public const ACCEPTED = 202;
        public const NONE_AUTHORITATIVE_INFORMATION = 203;
        public const NO_CONTENT = 204;
        public const RESET_CONTENT = 205;
        public const PARTIAL_CONTENT = 206;
        public const MULTI_STATUS = 207;
        public const ALREADY_REPORTED = 208;
        public const IM_USED = 226;
        public const SPECIAL_RESPONSE = 300;
        public const MOVED_PERMANENTLY = 301;
        public const MOVED_TEMPORARILY = 302;
        public const SEE_OTHER = 303;
        public const NOT_MODIFIED = 304;
        public const TEMPORARY_REDIRECT = 307;
        public const PERMANENT_REDIRECT = 308;
        public const BAD_REQUEST = 400;
        public const UNAUTHORIZED = 401;
        public const PAYMENT_REQUIRED = 402;
        public const FORBIDDEN = 403;
        public const NOT_FOUND = 404;
        public const NOT_ALLOWED = 405;
        public const NOT_ACCEPTABLE = 406;
        public const PROXY_AUTHENTICATION_REQUIRED = 407;
        public const REQUEST_TIME_OUT = 408;
        public const CONFLICT = 409;
        public const GONE = 410;
        public const LENGTH_REQUIRED = 411;
        public const PRECONDITION_FAILED = 412;
        public const REQUEST_ENTITY_TOO_LARGE = 413;
        public const REQUEST_URI_TOO_LARGE = 414;
        public const UNSUPPORTED_MEDIA_TYPE = 415;
        public const REQUEST_RANGE_NOT_SATISFIABLE = 416;
        public const EXPECTATION_FAILED = 417;
        public const I_AM_A_TEAPOT = 418;
        public const MISDIRECTED_REQUEST = 421;
        public const UNPROCESSABLE_ENTITY = 422;
        public const LOCKED = 423;
        public const FAILED_DEPENDENCY = 424;
        public const TOO_EARLY = 425;
        public const UPGRADE_REQUIRED = 426;
        public const TOO_MANY_REQUESTS = 429;
        public const REQUEST_HEADER_FIELDS_TOO_LARGE = 431;
        public const UNAVAILABLE_FOR_LEGAL_REASONS = 451;
        public const INTERNAL_SERVER_ERROR = 500;
        public const NOT_IMPLEMENTED = 501;
        public const BAD_GATEWAY = 502;
        public const SERVICE_UNAVAILABLE = 503;
        public const GATEWAY_TIME_OUT = 504;
        public const VERSION_NOT_SUPPORTED = 505;
        public const VARIANT_ALSO_NEGOTIATES = 506;
        public const INSUFFICIENT_STORAGE = 507;
        public const LOOP_DETECTED = 508;
        public const NOT_EXTENDED = 510;
        public const NETWORK_AUTHENTICATION_REQUIRED = 511;

        public static function getReasonPhraseFor(int $statusCode): string { }
    }
}

namespace Swow\Http
{
    class Parser
    {
        public const TYPE_BOTH = 0;
        public const TYPE_REQUEST = 1;
        public const TYPE_RESPONSE = 2;
        public const EVENT_FLAG_NONE = 0;
        public const EVENT_FLAG_LINE = 2;
        public const EVENT_FLAG_DATA = 4;
        public const EVENT_NONE = 0;
        public const EVENT_MESSAGE_BEGIN = 512;
        public const EVENT_URL = 1030;
        public const EVENT_URL_COMPLETE = 2056;
        public const EVENT_STATUS = 4102;
        public const EVENT_STATUS_COMPLETE = 8200;
        public const EVENT_HEADER_FIELD = 16388;
        public const EVENT_HEADER_VALUE = 32772;
        public const EVENT_HEADER_FIELD_COMPLETE = 65544;
        public const EVENT_HEADER_VALUE_COMPLETE = 131080;
        public const EVENT_HEADERS_COMPLETE = 262152;
        public const EVENT_BODY = 524292;
        public const EVENT_MESSAGE_COMPLETE = 1048584;
        public const EVENT_CHUNK_HEADER = 2097152;
        public const EVENT_CHUNK_COMPLETE = 4194312;
        public const EVENT_MULTIPART_DATA_BEGIN = 8388624;
        public const EVENT_MULTIPART_HEADER_FIELD = 16777236;
        public const EVENT_MULTIPART_HEADER_VALUE = 33554452;
        public const EVENT_MULTIPART_HEADERS_COMPLETE = 67108888;
        public const EVENT_MULTIPART_BODY = 134217748;
        public const EVENT_MULTIPART_DATA_END = 268435480;
        public const EVENTS_NONE = 0;
        public const EVENTS_ALL = 536870430;

        public function getType(): int { }

        public function setType(int $type): static { }

        public function getEvents(): int { }

        public function setEvents(int $events): static { }

        /** @param mixed $data */
        public function execute(\Swow\Buffer $buffer, &$data = null): int { }

        public function getEvent(): int { }

        public function getEventName(): string { }

        public function getDataOffset(): int { }

        public function getDataLength(): int { }

        public function getParsedLength(): int { }

        public function isCompleted(): bool { }

        public function shouldKeepAlive(): bool { }

        public function getMethod(): string { }

        public function getMajorVersion(): int { }

        public function getMinorVersion(): int { }

        public function getProtocolVersion(): string { }

        public function getStatusCode(): int { }

        public function getReasonPhrase(): string { }

        public function getContentLength(): int { }

        public function getCurrentChunkLength(): int { }

        public function isChunked(): bool { }

        public function isMultipart(): bool { }

        public function isUpgrade(): bool { }

        public function finish(): static { }

        public function reset(): static { }
    }
}

namespace Swow\Http
{
    class ParserException extends \Swow\Exception { }
}

namespace Swow\Http
{
    /**
     * pack up message into string without method line or status code line
     *
     * @param array<string, string|array<string>> $headers values indexed by fields
     */
    function packMessage(array $headers = [], string $body = ''): string { }
}

namespace Swow\Http
{
    /**
     * pack up request into string
     *
     * @param array<string, string|array<string>> $headers values indexed by fields
     */
    function packRequest(string $method = '', string $url = '', array $headers = [], string $body = '', string $protocolVersion = ''): string { }
}

namespace Swow\Http
{
    /**
     * pack up response into string
     *
     * @param array<string, string|array<string>> $headers values indexed by fields
     */
    function packResponse(int $statusCode = 0, array $headers = [], string $body = '', string $reasonPhrase = '', string $protocolVersion = ''): string { }
}

namespace Swow\WebSocket
{
    class Opcode
    {
        public const CONTINUATION = 0;
        public const TEXT = 1;
        public const BINARY = 2;
        public const CLOSE = 8;
        public const PING = 9;
        public const PONG = 10;

        public static function getNameFor(int $opcode): int { }
    }
}

namespace Swow\WebSocket
{
    class Status
    {
        public const NORMAL_CLOSURE = 1000;
        public const GOING_AWAY = 1001;
        public const PROTOCOL_ERROR = 1002;
        public const UNSUPPORTED_DATA = 1003;
        public const NO_STATUS_RECEIVED = 1005;
        public const ABNORMAL_CLOSURE = 1006;
        public const INVALID_FRAME_PAYLOAD_DATA = 1007;
        public const POLICY_VIOLATION = 1008;
        public const MESSAGE_TOO_BIG = 1009;
        public const MISSING_EXTENSION = 1010;
        public const INTERNAL_ERROR = 1011;
        public const SERVICE_RESTART = 1012;
        public const TRY_AGAIN_LATER = 1013;
        public const BAD_GATEWAY = 1014;
        public const TLS_HANDSHAKE = 1015;

        public static function getDescriptionFor(int $code): int { }
    }
}

namespace Swow\WebSocket
{
    class Frame implements \Stringable
    {
        public const PING = '8900';
        public const PONG = '8a00';

        public function unpackHeader(\Swow\Buffer $buffer): int { }

        public function resetHeader(): static { }

        public function getOpcode(): int { }

        public function setOpcode(int $opcode): static { }

        public function getFin(): bool { }

        public function setFin(int $fin): static { }

        public function getRSV1(): bool { }

        public function setRSV1(int $rsv1): static { }

        public function getRSV2(): bool { }

        public function setRSV2(int $rsv2): static { }

        public function getRSV3(): bool { }

        public function setRSV3(int $rsv3): static { }

        public function getHeaderLength(): int { }

        public function getPayloadLength(): int { }

        public function setPayloadLength(int $payloadLength): static { }

        public function getMask(): bool { }

        public function setMask(bool $mask): static { }

        public function getMaskKey(): string { }

        public function setMaskKey(string $maskKey): static { }

        public function hasPayloadData(): bool { }

        public function getPayloadData(): \Swow\Buffer { }

        public function getPayloadDataAsString(): string { }

        public function setPayloadData(?\Swow\Buffer $buffer): static { }

        public function unmaskPayloadData(): static { }

        public function toString(bool $headerOnly = false): string { }

        public function __toString(): string { }

        /** @return array<string, mixed> debug information for var_dump */
        public function __debugInfo(): array { }
    }
}

namespace Swow\Debug
{
    /**
     * build trace info from `\Swow\Coroutine::getTrace()` returns
     *
     * @see \Swow\Coroutine::getTrace()
     * @param array<array{'file': string, 'line': int, 'function': string, 'class': string, 'type': string, 'args': array<mixed>}> $trace
     */
    function buildTraceAsString(array $trace): string { }
}

namespace Swow\Debug
{
    function registerExtendedStatementHandler(callable $handler): \Swow\Util\Handler { }
}
