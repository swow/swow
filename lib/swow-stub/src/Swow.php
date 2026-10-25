<?php

/** @noinspection PhpUnused, PhpInconsistentReturnPointsInspection, PhpMissingParentConstructorInspection, PhpReturnDocTypeMismatchInspection, PhpMixedReturnTypeCanBeReducedInspection */

namespace
{
    !defined('AF_INET') && define('AF_INET', 2);
    !defined('AF_INET6') && define('AF_INET6', 30);
    !defined('AF_UNSPEC') && define('AF_UNSPEC', 0);
    const STREAM_POLLNONE = 0;
    const STREAM_POLLIN = 1;
    const STREAM_POLLPRI = 2;
    const STREAM_POLLOUT = 4;
    const STREAM_POLLERR = 8;
    const STREAM_POLLHUP = 16;
    const STREAM_POLLNVAL = 32;
}

namespace
{
    class Swow
    {
        /**
         * Check whether the swow/swow-library package is loaded.
         *
         * The method looks up the class Swow\Library.
         *
         * @return bool true when the package is loaded
         */
        public static function isLibraryLoaded(): bool { }
    }
}

namespace
{
    /**
     * sleep milliseconds
     *
     * @param int $milli_seconds time to sleep, in ms
     * @return int always 0
     * @throws \ValueError when the value is negative
     */
    function msleep(int $milli_seconds): int { }
}

namespace
{
    /**
     * Resolve a host name to an IP address.
     *
     * @return string the resolved address
     * @throws \RuntimeException when the name does not resolve. The code is a negative EAI code.
     */
    function gethostbyname2(string $hostname, int $address_family = \AF_INET): string { }
}

namespace
{
    /**
     * unlimited version of stream_select()
     *
     * @param array<resource> $read
     * @param array<resource> $write
     * @param array<resource> $except
     * @param int|null $seconds
     * @param int|null $microseconds
     * @return int|false number of stream resources that are ready for reading, writing, or have an error, or false on error
     * @throws \ValueError when no valid stream remains in the arrays
     */
    function stream_select_unlimited(?array &$read, ?array &$write, ?array &$except, ?int $seconds, ?int $microseconds = null): int|false { }
}

namespace
{
    /**
     * poll one stream for events
     *
     * @param resource $stream stream resource
     * @param int $events flags: STREAM_POLL* constants combinations
     * @return int flags: STREAM_POLL* constants combinations, STREAM_POLLNONE means timeout.
     * The result can contain STREAM_POLLHUP and STREAM_POLLERR.
     * @throws RuntimeException on runtime error, e.g. poll has been cancelled or poll failed
     */
    function stream_poll_one($stream, int $events, int $timeout = -1): int { }
}

namespace Swow
{
    const NPROC_AVAILABLE = 0;
}

namespace Swow
{
    class Extension
    {
        public const VERSION = '2.0.0-dev';
        public const VERSION_ID = 20000;
        public const MAJOR_VERSION = 2;
        public const MINOR_VERSION = 0;
        public const RELEASE_VERSION = 0;
        public const EXTRA_VERSION = 'dev';

        /**
         * Check whether the extension was built with the library.
         *
         * The names are case-insensitive: "ssl", "openssl", "curl", "pgsql", "mongodb", "debug",
         * "debug_log", "asan", "msan", "ubsan".
         *
         * @return bool true when the library is available in this build
         */
        public static function isBuiltWith(string $lib): bool { }
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
        public const ENODATA = -61;
        public const EUNATCH = -49;
        public const ENOEXEC = -8;
        public const UNCODED = -9763;
        public const EPREV = -9762;
        public const EMISUSE = -9761;
        public const EVALUE = -9760;
        public const ELOCKED = -9759;
        public const ECLOSED = -9758;
        public const EDEADLK = -9757;
        public const ESSL = -9756;
        public const ENOCERT = -9755;
        public const ECERT = -9754;
        public const ECHILD = -9753;
        public const EHP_OK = -816000;
        public const EHP_INTERNAL = -816001;
        public const EHP_STRICT = -816002;
        public const EHP_CR_EXPECTED = -816025;
        public const EHP_LF_EXPECTED = -816003;
        public const EHP_UNEXPECTED_CONTENT_LENGTH = -816004;
        public const EHP_UNEXPECTED_SPACE = -816030;
        public const EHP_CLOSED_CONNECTION = -816005;
        public const EHP_INVALID_METHOD = -816006;
        public const EHP_INVALID_URL = -816007;
        public const EHP_INVALID_CONSTANT = -816008;
        public const EHP_INVALID_VERSION = -816009;
        public const EHP_INVALID_HEADER_TOKEN = -816010;
        public const EHP_INVALID_CONTENT_LENGTH = -816011;
        public const EHP_INVALID_CHUNK_SIZE = -816012;
        public const EHP_INVALID_STATUS = -816013;
        public const EHP_INVALID_EOF_STATE = -816014;
        public const EHP_INVALID_TRANSFER_ENCODING = -816015;
        public const EHP_CB_MESSAGE_BEGIN = -816016;
        public const EHP_CB_HEADERS_COMPLETE = -816017;
        public const EHP_CB_MESSAGE_COMPLETE = -816018;
        public const EHP_CB_CHUNK_HEADER = -816019;
        public const EHP_CB_CHUNK_COMPLETE = -816020;
        public const EHP_PAUSED = -816021;
        public const EHP_PAUSED_UPGRADE = -816022;
        public const EHP_PAUSED_H2_UPGRADE = -816023;
        public const EHP_USER = -816024;
        public const EHP_CB_URL_COMPLETE = -816026;
        public const EHP_CB_STATUS_COMPLETE = -816027;
        public const EHP_CB_METHOD_COMPLETE = -816032;
        public const EHP_CB_VERSION_COMPLETE = -816033;
        public const EHP_CB_HEADER_FIELD_COMPLETE = -816028;
        public const EHP_CB_HEADER_VALUE_COMPLETE = -816029;
        public const EHP_CB_CHUNK_EXTENSION_NAME_COMPLETE = -816034;
        public const EHP_CB_CHUNK_EXTENSION_VALUE_COMPLETE = -816035;
        public const EHP_CB_RESET = -816031;
        public const EHP_NOMEM = -816065;
        public const EHP_MULTIPART = -816066;

        /**
         * Get the description of an errno value.
         *
         * A positive value is converted to the negative system error first.
         *
         * @return string the description. 0 and unknown values give "Unknown error".
         * @throws \ValueError when the value is out of the errno range
         */
        public static function getDescriptionOf(int $error): string { }
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

        /**
         * get enabled log types
         *
         * @return int the log type flag
         */
        public static function getTypes(): int { }

        /**
         * set logging types
         *
         * @param int $types a combination of the TYPE_* constants
         * @throws \ValueError when the value is outside TYPES_ALL
         */
        public static function setTypes(int $types): void { }
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
    /**
     * Coroutine class is the abstract of [coroutine](https://en.wikipedia.org/wiki/Coroutine).
     *
     * A coroutine is a subroutine actively suspend itself, and get resumed when there is no other running coroutines, it's co-operative so it was called co-routine.
     *
     * We call suspending self "yield", when yielding form a coroutine, the coroutine suspends its execution, the scheduler will find and execute another coroutine can continue.
     *
     * Swow have hooked most of IO operation functions in PHP, an IO coroutine will yield when calling these functions. When the IO operation is done, and current coroutine yield, the scheduler may continue the fore-mentioned IO coroutine. This is how Swow make IO concurrency.
     */
    class Coroutine
    {
        public const STATE_NONE = 0;
        public const STATE_WAITING = 1;
        public const STATE_RUNNING = 2;
        public const STATE_DEAD = 3;

        public function __construct(callable $callable) { }

        /**
         * Create a coroutine and run it immediately
         *
         * An uncaught exception in the body does not reach the caller. The coroutine ends with exit status 0.
         *
         * @note context swap happens here
         *
         * @param callable $callable the coroutine body
         * @param mixed ...$data arguments passed to the callable
         * @return static the new created coroutine
         */
        public static function run(callable $callable, mixed ...$data): static { }

        /**
         * Resume coroutine execution
         *
         * @note context swap happens here when success
         * @return mixed the value of the next yield(), or the return value of the body
         */
        public function resume(mixed ...$data): mixed { }

        /**
         * Yield from current coroutine
         *
         * The coroutine continues only after another coroutine calls resume().
         * A yield in the main coroutine without a resumer stops the process.
         *
         * @note context swap happens here when success
         * @return mixed the value passed to the next resume()
         */
        public static function yield(mixed $data = null): mixed { }

        /** Get unique coroutine id */
        public function getId(): int { }

        /** Get current coroutine */
        public static function getCurrent(): self|static { }

        /** Get main (the most outter) coroutine */
        public static function getMain(): self|static { }

        /** Get previous coroutine */
        public function getPrevious(): self|static { }

        /**
         * Get coroutine state
         *
         * @return int the enum number: static::STATE_* constants
         */
        public function getState(): int { }

        public function getStateName(): string { }

        public function getSwitches(): int { }

        public static function getGlobalSwitches(): int { }

        public function getStartTime(): int { }

        public function getEndTime(): int { }

        public function getElapsed(): int { }

        public function getElapsedAsString(): string { }

        /**
         * Get the exit status.
         *
         * @return int 0 after a normal return, a kill or an uncaught exception. The main coroutine returns the request exit status.
         */
        public function getExitStatus(): int { }

        public function isAvailable(): bool { }

        public function isAlive(): bool { }

        /**
         * Check whether the coroutine started and did not end.
         *
         * A suspended coroutine counts as executing.
         *
         * @return bool true when the coroutine started and did not end
         */
        public function isExecuting(): bool { }

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
         * The stack is the current PHP stack. Before the first switch it contains the caller frames.
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
         * @return int the trace depth, -1 before the coroutine starts
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
         * The code is one expression. The implementation adds "return " before the code.
         * A return statement or an echo statement is a parse error.
         *
         * @TODO string $string? why not rename it to $code
         * @param string $string expression to evaluate
         * @return mixed the value of the expression
         * @throws CoroutineException when the code is invalid
         * @see Coroutine::getTrace() for `$level` params usage
         */
        public function eval(string $string, int $level = 0): mixed { }

        /**
         * Call a function in the scope of a frame.
         *
         * @return mixed the return value of the function
         */
        public function call(callable $callable, int $level = 0): mixed { }

        /**
         * Throw an exception into the coroutine and resume it.
         *
         * @return mixed the return value of the body
         */
        public function throw(\Throwable $throwable): mixed { }

        public function kill(): void { }

        public static function killAll(): void { }

        /**
         * Get the number of coroutines.
         *
         * @return int the number of coroutines, including the main coroutine
         */
        public static function count(): int { }

        /**
         * Get a coroutine by id.
         *
         * @return static|null the coroutine, or null when the id is not found
         */
        public static function get(int $id): ?static { }

        /**
         * Get all coroutines.
         *
         * @return array<int, Coroutine> all coroutines, including the main coroutine
         */
        public static function getAll(): array { }

        /** @return array<string, mixed> debug information for var_dump */
        public function __debugInfo(): array { }

        /**
         * Register a callback for a deadlock.
         *
         * The scheduler calls the callback when all coroutines wait. The process stays blocked after the call.
         * The callback must kill coroutines or stop the process.
         *
         * @return Utils\Handler handler for remove()
         */
        public static function registerDeadlockHandler(callable $callable): Utils\Handler { }
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
        /**
         * Create a channel.
         *
         * @param int $capacity 0 is unbuffered. -1 is unlimited.
         * @throws \ValueError when the capacity is negative and not -1
         */
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
         * @param int $timeout in milliseconds
         * @return static
         * @throws ChannelException on timeout (-110) or when the channel is closed (-9758)
         */
        public function push(mixed $data, int $timeout = -1): static { }

        /**
         * pop data from channel
         *
         * @note context switching happens here when channel is empty, context switching may also happen at this even channel is not empty.
         *
         * @param int $timeout in milliseconds
         * @phan-return T $data
         * @phpstan-return T $data
         * @psalm-return T $data
         * @return mixed
         * @throws ChannelException on timeout (-110) or when the channel is closed (-9758)
         */
        public function pop(int $timeout = -1): mixed { }

        /**
         * Close the channel.
         *
         * A queued value can still be popped. A waiting coroutine wakes with error code -125.
         *
         * @throws ChannelException when the channel is already closed
         */
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
        public const EVENT_PUSH = 0;
        public const EVENT_POP = 1;

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
         * The first ready action wins. With no ready action the call waits for the timeout.
         *
         * @note context switching may happen here
         *
         * @param int $timeout timeout in milliseconds
         * @throws SelectorException on timeout or failure
         * @phan-return \Swow\Channel<T>
         * @phpstan-return \Swow\Channel<T>
         * @psalm-return \Swow\Channel<T>
         */
        public function commit(int $timeout = -1): Channel { }

        /**
         * Get the value of the last pop event.
         *
         * @return mixed the value
         * @throws \Error when no data is available: a push event, no commit, or a failed commit
         */
        public function fetch(): mixed { }

        /**
         * Get the event of the last successful commit.
         *
         * @return int the event, or EVENT_NONE
         */
        public function getLastEvent(): int { }
    }
}

namespace Swow
{
    class SelectorException extends \Swow\CallException { }
}

namespace Swow
{
    class SyncException extends \Swow\Exception { }
}

namespace Swow
{
    /**
     * An OS thread with its own PHP request.
     *
     * The thread body and its arguments are serialized at start.
     * The child request has no user code, no autoloader and no global state.
     * A closure cannot be serialized, so a closure body cannot start.
     * Use an internal function or a method of a Swow object that shares state by id.
     */
    class Thread
    {
        /**
         * Create a thread.
         *
         * This method does not start the thread. This method does not yield.
         *
         * @param callable $callable The thread body.
         * @param mixed ...$args Arguments for the thread body.
         */
        public function __construct(callable $callable, mixed ...$args) { }

        /**
         * Create and start a thread.
         *
         * The body and the arguments are serialized. See the class description.
         * This method waits for the child runtime. In a coroutine, this method suspends the current coroutine.
         *
         * @param callable $callable The thread body.
         * @param mixed ...$args Arguments for the thread body.
         * @throws ThreadException when the thread cannot start.
         * @throws \Exception when the body or an argument cannot be serialized.
         */
        public static function run($callable, ...$args): self { }

        /**
         * Create and start a thread that runs PHP code.
         *
         * The code runs as top level code in a fresh request. Parent globals are not visible.
         * The code has no $context variable.
         * The return value and the exceptions of the code are discarded. Read getExitStatus() for the result.
         * This method waits for the child runtime. In a coroutine, this method suspends the current coroutine.
         *
         * @param string $code PHP code without an opening tag
         * @throws ThreadException when the thread cannot start.
         */
        public static function runCode(string $code): self { }

        /**
         * Start the thread.
         *
         * This method waits for the child runtime. In a coroutine, this method suspends the current coroutine.
         * Do not call this method twice. A second call throws ThreadException.
         *
         * @param int $timeout timeout in milliseconds, -1 waits forever
         * @throws ThreadException when the thread cannot start or the wait times out.
         */
        public function start(int $timeout = -1): self { }

        /**
         * Wait for the thread to exit.
         *
         * A later call returns the cached result.
         * A fatal error in the body gives status 255.
         *
         * @note context switching will happen here
         *
         * @param int $timeout timeout in milliseconds, -1 waits forever
         * @return mixed the return value of the body, or null after a fatal error
         * @throws ThreadException when the wait times out, the thread was killed, or the thread was detached.
         * @throws \Throwable the exception that the thread body threw
         */
        public function wait(int $timeout = -1): mixed { }

        /**
         * Stop waiting for the thread.
         *
         * RSHUTDOWN joins the thread later. Do not call wait after this method.
         * This method does not yield.
         */
        public function detach(): self { }

        /**
         * Send an asynchronous kill request.
         *
         * This method does not wait for the thread to exit.
         * The thread stops at the next safe point. A blocking native call completes first.
         * After the kill the status is -1.
         * Call wait to get the result.
         *
         * @throws ThreadException when the thread is not running.
         */
        public function kill(): self { }

        /**
         * Send a kill request to every child thread.
         *
         * This method does not wait for the threads to exit.
         *
         * @return int the number of threads that accepted the request
         */
        public static function killAll(): int { }

        /** Return true after start was called. */
        public function isStarted(): bool { }

        /** Return true while the thread is running. */
        public function isRunning(): bool { }

        /** Return true after detach was called. */
        public function isDetached(): bool { }

        /**
         * Return the native thread id.
         *
         * The value is fixed at start and does not change after join.
         * The OS can reuse the id after the thread exits.
         * This method returns 0 before start.
         */
        public function getId(): int { }

        /** Return debug information for var_dump. */
        public function __debugInfo(): array { }

        /**
         * Return the Thread object for the current thread.
         *
         *
         * @throws ThreadException when Swow has no current thread.
         */
        public static function getCurrent(): self { }

        /**
         * Return the native id of the current thread.
         *
         * This method returns 0 when Swow has no current thread.
         */
        public static function getCurrentId(): int { }

        /**
         * Return the exit status of the thread.
         *
         * The status is 0 for a normal return, N for exit(N), 255 for an uncaught exception, and -1 for a kill.
         */
        public function getExitStatus(): int { }
    }
}

namespace Swow
{
    /** Report Thread API errors. */
    class ThreadException extends \Swow\Exception { }
}

namespace Swow
{
    /**
     * A mutex that works across threads.
     *
     * The mutex is not recursive.
     * The owner is a thread. A mutex stays locked when the owner thread exits.
     * Do not destroy a locked mutex. Destruction of a locked mutex emits a warning.
     */
    class Mutex
    {
        /** Create a mutex. */
        public function __construct() { }

        /**
         * Take the mutex.
         *
         * In a coroutine, this method suspends the current coroutine when the mutex is held.
         * Do not lock the same mutex twice from one thread. The second call waits and can deadlock.
         *
         * @note context switching may happen here
         *
         * @param int $timeout timeout in milliseconds, -1 waits forever
         * @return bool false on timeout
         * @throws \Swow\SyncException when the caller cannot wait for the mutex.
         */
        public function lock(int $timeout = -1): bool { }

        /**
         * Release the mutex.
         *
         * The caller must hold the mutex.
         *
         * @throws \Swow\SyncException when the caller does not hold the mutex.
         */
        public function unlock(): void { }

        /**
         * Serialize the mutex by shared object id.
         *
         * The serialized form does not copy the mutex state.
         *
         * @throws \Swow\SyncException when the mutex is not constructed.
         */
        public function __serialize(): array { }

        /**
         * Restore the mutex from a shared object id.
         *
         * @throws \Swow\SyncException when the data is invalid or the object is gone.
         */
        public function __unserialize(array $data): void { }

        /** Return debug information. */
        public function __debugInfo(): array { }
    }
}

namespace Swow
{
    /** A counting semaphore that works across threads. */
    class Semaphore
    {
        /**
         * Create a semaphore. This method does not yield.
         *
         * @param int $value initial permit count, from 0 to UINT_MAX
         * @throws \ValueError when the value is out of range.
         * @throws \Swow\SyncException when the semaphore is already constructed.
         */
        public function __construct(int $value = 0) { }

        /**
         * Take one permit.
         *
         * In a coroutine, this method suspends the current coroutine when the count is zero.
         *
         * @note context switching may happen here
         *
         * @param int $timeout timeout in milliseconds, -1 waits forever
         * @return bool false on timeout
         * @throws \Swow\SyncException when the caller cannot wait for the semaphore.
         */
        public function acquire(int $timeout = -1): bool { }

        /**
         * Try to take one permit without waiting.
         *
         * @return bool true when a permit was taken
         */
        public function tryAcquire(): bool { }

        /** Return the current permit count. */
        public function getValue(): int { }

        /** Return the number of waiters. */
        public function getWaiterCount(): int { }

        /**
         * Add permits and wake waiters.
         *
         * A count of 0 does nothing.
         *
         * @param int $count number of permits to add
         * @throws \ValueError when the count is negative.
         */
        public function release(int $count = 1): void { }

        /**
         * Serialize the semaphore by shared object id.
         *
         * The serialized form does not copy the semaphore state.
         *
         * @throws \Swow\SyncException when the semaphore is not constructed.
         */
        public function __serialize(): array { }

        /**
         * Restore the semaphore from a shared object id.
         *
         * @throws \Swow\SyncException when the data is invalid or the object is gone.
         */
        public function __unserialize(array $data): void { }
    }
}

namespace Swow
{
    /** Report Thannel API errors. */
    class ThannelException extends \Swow\Exception { }
}

namespace Swow
{
    /**
     * A queue that transfers serialized values between threads.
     *
     * The capacity is a value count. Capacity 0 is unbuffered. Capacity -1 is unlimited.
     * Values are serialized. A resource becomes 0. A closure cannot be passed.
     * An unbuffered push waits for the consumer. In one OS thread the push does not return after the take.
     * Use an unbuffered Thannel only across threads.
     */
    class Thannel
    {
        /**
         * Create a Thannel. This method does not yield.
         *
         * @param int $capacity capacity is -1 or between 0 and UINT32_MAX
         * @throws \ValueError when the capacity is out of range.
         * @throws ThannelException when the Thannel is already constructed.
         */
        public function __construct(int $capacity = 0) { }

        /**
         * Serialize a value and add it to the queue.
         *
         * In a coroutine, this method suspends the current coroutine when the queue is full.
         * An unbuffered push in one OS thread does not return after the take. The timeout does not fire.
         *
         * @note context switching may happen here
         *
         * @param mixed $data value to serialize
         * @param int $timeout timeout in milliseconds, -1 waits forever
         * @throws ThannelException on timeout (-110) or when the queue is closed (-9758)
         * @throws \Exception when the value cannot be serialized
         */
        public function push($data, int $timeout = -1): self { }

        /**
         * Remove a value and unserialize it.
         *
         * In a coroutine, this method suspends the current coroutine when the queue is empty.
         *
         * @note context switching may happen here
         *
         * @param int $timeout timeout in milliseconds, -1 waits forever
         * @throws ThannelException when the wait times out, the queue is closed and empty, or the value cannot be unserialized.
         */
        public function pop(int $timeout = -1): mixed { }

        /**
         * Serialize a batch and push it as one unit.
         *
         * The batch enters as a whole or not at all. In a coroutine, this method suspends the current coroutine
         * when the batch does not fit.
         *
         * @note context switching may happen here
         *
         * @param array $values values to serialize
         * @param int $timeout timeout in milliseconds, -1 waits forever
         * @return int number of values pushed
         * @throws ThannelException on timeout, a closed queue, or an oversized batch
         */
        public function pushMany(array $values, int $timeout = -1): int { }

        /**
         * Remove up to max values.
         *
         * The call returns at least one value when the queue is not empty.
         *
         * @note context switching may happen here
         *
         * @param int $max maximum number of values to return
         * @param int $timeout timeout in milliseconds, -1 waits forever
         * @return array the values in queue order
         */
        public function popMany(int $max, int $timeout = -1): array { }

        /** Remove all queued values without waiting. */
        public function drain(): array { }

        /** Return the number of wakes that reached a waiter. */
        public function getWakeCount(): int { }

        /** Return the number of times a side parked. */
        public function getParkCount(): int { }

        /**
         * Close the queue.
         *
         * Queued values stay available. A later push is rejected. A second close() does nothing.
         *
         * @throws ThannelException when the queue is not constructed or close fails.
         */
        public function close(): void { }

        /** Return true after the queue is closed. */
        public function isClosed(): bool { }

        /** Return the queue capacity. */
        public function getCapacity(): int { }

        /** Return the number of queued values. */
        public function getLength(): int { }

        /** Return true when the queue has no values. */
        public function isEmpty(): bool { }

        /** Return true when the queue is full. An unbuffered Thannel is always full. */
        public function isFull(): bool { }

        /** @return bool true when the queue is empty or full. An unbuffered Thannel always returns true. */
        public function willBlock(): bool { }

        /**
         * Serialize the Thannel by shared object id.
         *
         * The serialized form does not copy the queue.
         *
         * @throws ThannelException when the Thannel is not constructed.
         */
        public function __serialize(): array { }

        /**
         * Restore the Thannel from a shared object id.
         *
         * @throws ThannelException when the data is invalid or the object is gone.
         */
        public function __unserialize(array $data): void { }

        /** Return debug information. */
        public function __debugInfo(): array { }
    }
}

namespace Swow
{
    /** Report SpscQueue API errors. */
    class SpscQueueException extends \Swow\Exception { }
}

namespace Swow
{
    /**
     * A fixed-size single-producer single-consumer queue in shared memory.
     *
     * One thread produces and one thread consumes. A blocking call stops the whole OS thread,
     * so use it only in a worker that runs one coroutine.
     * An anonymous queue is visible to threads of this process and to a forked child.
     * A named queue is visible to another process that attaches the same name.
     */
    class SpscQueue
    {
        /**
         * Create an anonymous queue.
         *
         * @param int $capacity maximum number of queued values
         * @param int $maxMessageSize maximum size of one serialized value in bytes
         * @throws \ValueError when a size is out of range
         * @throws SpscQueueException when the queue cannot be created
         */
        public function __construct(int $capacity, int $maxMessageSize = 8192) { }

        /**
         * Create a named queue.
         *
         * @throws \ValueError when a size is out of range
         * @throws SpscQueueException when the name exists or the queue cannot be created
         */
        public static function createNamed(string $name, int $capacity, int $maxMessageSize = 8192): self { }

        /** Attach a named queue. */
        public static function attachNamed(string $name): self { }

        /**
         * Remove a name.
         *
         * An existing mapping stays valid. On Windows the section ends when the last mapping
         * closes; unlink does not remove the name earlier.
         */
        public static function unlink(string $name): void { }

        /** Return the name, or null for an anonymous queue. */
        public function getName(): ?string { }

        /**
         * Serialize a value and push it.
         *
         * @note this method blocks the whole OS thread
         *
         * @param mixed $value value to serialize
         * @param int $timeout timeout in milliseconds, -1 waits forever
         * @throws SpscQueueException on timeout, a closed queue, or a serialization failure
         */
        public function push($value, int $timeout = -1): self { }

        /**
         * Pop and unserialize a value.
         *
         * @note this method blocks the whole OS thread
         *
         * @param int $timeout timeout in milliseconds, -1 waits forever
         * @throws SpscQueueException on timeout, a closed empty queue, or a deserialization failure
         */
        public function pop(int $timeout = -1): mixed { }

        /**
         * Serialize a batch and push it as one unit.
         *
         * The batch enters as a whole or not at all.
         *
         * @note this method blocks the whole OS thread
         *
         * @param array $values values to serialize
         * @param int $timeout timeout in milliseconds, -1 waits forever
         * @return int number of values pushed
         * @throws SpscQueueException on timeout, a closed queue, or an oversized batch
         */
        public function pushMany(array $values, int $timeout = -1): int { }

        /**
         * Pop up to max values.
         *
         * The call returns at least one value when the queue is not empty.
         *
         * @note this method blocks the whole OS thread
         *
         * @param int $max maximum number of values to return
         * @param int $timeout timeout in milliseconds, -1 waits forever
         * @return array the values in queue order
         */
        public function popMany(int $max, int $timeout = -1): array { }

        /** Pop all queued values without waiting. */
        public function drain(): array { }

        /** Stop further pushes and wake both sides. */
        public function close(): void { }

        /** Return true after close. */
        public function isClosed(): bool { }

        /** Return the maximum number of queued values. */
        public function getCapacity(): int { }

        /** Return the maximum serialized value size in bytes. */
        public function getMaxMessageSize(): int { }

        /** Return the number of queued values. */
        public function getLength(): int { }

        /** Return true when the queue has no values. */
        public function isEmpty(): bool { }

        /** Return true when the queue has no free slot. */
        public function isFull(): bool { }

        /** Return the number of wakes that reached a waiter. */
        public function getWakeCount(): int { }

        /** Return the number of times a side parked. */
        public function getParkCount(): int { }

        /**
         * Serialize the queue by name or shared object id.
         *
         * The serialized form does not copy queued values.
         */
        public function __serialize(): array { }

        /** Restore the queue from a name or shared object id. */
        public function __unserialize(array $data): void { }

        /** Return debug information. */
        public function __debugInfo(): array { }
    }
}

namespace Swow
{
    /**
     * A shared memory byte buffer with named and anonymous mappings.
     *
     * An anonymous buffer is visible to threads of this process and to a forked child.
     * A named buffer is visible to another process that attaches the same name.
     * The buffer does not lock. The caller coordinates access.
     * The typed methods use offsetBytes as a byte offset and count as an element count.
     * The endian argument selects the byte order.
     */
    class SharedBuffer
    {
        /** Use the byte order of this machine. */
        public const NATIVE_ENDIAN = 0;
        /** Store and load typed values in little-endian order. */
        public const LITTLE_ENDIAN = 1;
        /** Store and load typed values in big-endian order. */
        public const BIG_ENDIAN = 2;

        /**
         * Create an anonymous buffer.
         *
         * @param int $size size in bytes
         * @param int $alignment alignment in bytes, a power of two from 8 to 4096
         * @throws \ValueError when the size is not positive or the alignment is not valid
         * @throws \Swow\SyncException when the buffer cannot be created
         */
        public function __construct(int $size, int $alignment = 64) { }

        /**
         * Create a named buffer.
         *
         * @param string $name mapping name
         * @param int $size size in bytes
         * @param int $alignment alignment in bytes, a power of two from 8 to 4096
         * @param int $flags bit 0 removes the name when the last handle releases the buffer
         * @throws \ValueError when the alignment is not valid
         * @throws \Swow\SyncException when the name exists or the buffer cannot be created
         */
        public static function createNamed(string $name, int $size, int $alignment = 64, int $flags = 0): self { }

        /** Attach a named buffer. */
        public static function attachNamed(string $name): self { }

        /** Remove a name. An existing mapping stays valid. */
        public static function unlink(string $name): void { }

        /** Return the buffer size in bytes. */
        public function getSize(): int { }

        /** Return the name, or null for an anonymous buffer. */
        public function getName(): ?string { }

        /** Return the shared object id. */
        public function getSharedId(): int { }

        /**
         * Return the reference count of the mapping.
         *
         * The threads and the forked children that map the same buffer share the count,
         * so it is not the number of handles in this process.
         */
        public function getRefCount(): int { }

        /** Return the alignment in bytes. */
        public function getAlignment(): int { }

        /**
         * Return a view of a byte range of this buffer.
         *
         * The view holds a reference to this buffer, and it cannot be serialized.
         *
         * @param int $offset offset in bytes
         * @param int $size size in bytes
         * @throws \ValueError when the range is outside the buffer
         * @throws \Swow\SyncException when the view cannot be created
         */
        public function slice(int $offset, int $size): self { }

        /** Copy bytes out of the buffer. */
        public function read(int $offsetBytes, int $lengthBytes): string { }

        /** Copy bytes into the buffer. */
        public function write(int $offsetBytes, string $data): void { }

        /** Store 32-bit float values at a byte offset. */
        public function writeF32(int $offsetBytes, array $values, int $endian = 0): void { }

        /** Load 32-bit float values at a byte offset. */
        public function viewF32(int $offsetBytes, int $count, int $endian = 0): array { }

        /** Store 64-bit float values at a byte offset. */
        public function writeF64(int $offsetBytes, array $values, int $endian = 0): void { }

        /** Load 64-bit float values at a byte offset. */
        public function viewF64(int $offsetBytes, int $count, int $endian = 0): array { }

        /** Store signed 8-bit integers at a byte offset. */
        public function writeI8(int $offsetBytes, array $values, int $endian = 0): void { }

        /** Load signed 8-bit integers at a byte offset. */
        public function viewI8(int $offsetBytes, int $count, int $endian = 0): array { }

        /** Store unsigned 8-bit integers at a byte offset. */
        public function writeU8(int $offsetBytes, array $values, int $endian = 0): void { }

        /** Load unsigned 8-bit integers at a byte offset. */
        public function viewU8(int $offsetBytes, int $count, int $endian = 0): array { }

        /** Store signed 16-bit integers at a byte offset. */
        public function writeI16(int $offsetBytes, array $values, int $endian = 0): void { }

        /** Load signed 16-bit integers at a byte offset. */
        public function viewI16(int $offsetBytes, int $count, int $endian = 0): array { }

        /** Store unsigned 16-bit integers at a byte offset. */
        public function writeU16(int $offsetBytes, array $values, int $endian = 0): void { }

        /** Load unsigned 16-bit integers at a byte offset. */
        public function viewU16(int $offsetBytes, int $count, int $endian = 0): array { }

        /** Store signed 32-bit integers at a byte offset. */
        public function writeI32(int $offsetBytes, array $values, int $endian = 0): void { }

        /** Load signed 32-bit integers at a byte offset. */
        public function viewI32(int $offsetBytes, int $count, int $endian = 0): array { }

        /** Store unsigned 32-bit integers at a byte offset. */
        public function writeU32(int $offsetBytes, array $values, int $endian = 0): void { }

        /** Load unsigned 32-bit integers at a byte offset. */
        public function viewU32(int $offsetBytes, int $count, int $endian = 0): array { }

        /** Store signed 64-bit integers at a byte offset. */
        public function writeI64(int $offsetBytes, array $values, int $endian = 0): void { }

        /** Load signed 64-bit integers at a byte offset. */
        public function viewI64(int $offsetBytes, int $count, int $endian = 0): array { }

        /** Store unsigned 64-bit integers at a byte offset. */
        public function writeU64(int $offsetBytes, array $values, int $endian = 0): void { }

        /** Load unsigned 64-bit integers at a byte offset. */
        public function viewU64(int $offsetBytes, int $count, int $endian = 0): array { }

        /** Fill bytes with one value. */
        public function fill(int $offsetBytes, int $lengthBytes, int $byte): void { }

        /** Set all bytes to zero. */
        public function clear(): void { }

        /** Copy bytes between buffers. */
        public function copyFrom(self $source, int $sourceOffsetBytes, int $destinationOffsetBytes, int $lengthBytes): void { }

        /** Serialize the buffer by name or shared object id. */
        public function __serialize(): array { }

        /** Restore the buffer from a name or shared object id. */
        public function __unserialize(array $data): void { }

        /** Return debug information. */
        public function __debugInfo(): array { }
    }
}

namespace Swow
{
    /**
     * A 32-bit futex in shared memory.
     *
     * wait() blocks the whole OS thread, not only the current coroutine.
     * Use it only in a worker thread that runs one coroutine.
     * An anonymous futex is visible to threads of this process and to a forked child.
     * A named futex is visible to another process that attaches the same name.
     * setValue() publishes the value with release order. getValue() reads it with acquire order.
     * add(), compareExchange(), and exchange() are atomic.
     * Store a value before wake().
     */
    class Futex
    {
        /**
         * Create an anonymous futex.
         *
         * @param int $value initial signed 32-bit value
         * @throws \ValueError when the value does not fit
         * @throws \Swow\SyncException when the futex cannot be created
         */
        public function __construct(int $value = 0) { }

        /** Create a named futex. */
        public static function createNamed(string $name, int $value = 0): self { }

        /** Attach a named futex. */
        public static function attachNamed(string $name): self { }

        /** Remove a name. An existing mapping stays valid. */
        public static function unlink(string $name): void { }

        /** Return the current value with acquire order. */
        public function getValue(): int { }

        /** Store a value with release order. */
        public function setValue(int $value): void { }

        /**
         * Add a signed value to the futex word.
         *
         * The addition is atomic and wraps on overflow.
         *
         * @param int $delta signed 32-bit value to add
         * @return int the value before the addition
         */
        public function add(int $delta): int { }

        /**
         * Compare and exchange the futex word.
         *
         * The write happens only when the current value equals expected.
         *
         * @param int $expected signed 32-bit value to compare
         * @param int $new signed 32-bit value to store
         * @return bool true when the value was stored
         */
        public function compareExchange(int $expected, int $new): bool { }

        /**
         * Exchange the futex word.
         *
         * @param int $new signed 32-bit value to store
         * @return int the value before the exchange
         */
        public function exchange(int $new): int { }

        /**
         * Wait while the value equals expected.
         *
         * @note this method blocks the whole OS thread
         *
         * @param int $expected signed 32-bit value to compare
         * @param int $timeout timeout in milliseconds, -1 waits forever
         * @return bool false when the value changed or the timeout expired
         * @throws \Swow\SyncException when the wait cannot be performed
         */
        public function wait(int $expected, int $timeout = -1): bool { }

        /**
         * Wake waiters. This method does not block.
         *
         * A negative count wakes all waiters. A count of zero wakes none.
         */
        public function wake(int $count): bool { }

        /** Return the shared object id. */
        public function getSharedId(): int { }

        /** Serialize the futex by name or shared object id. */
        public function __serialize(): array { }

        /** Restore the futex from a name or shared object id. */
        public function __unserialize(array $data): void { }

        /** Return debug information. */
        public function __debugInfo(): array { }
    }
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
        public const COMMON_SIZE = 8192;

        /**
         * Round the size up to a multiple of the alignment.
         *
         * The alignment must be a power of two. Other values give a result that is not a multiple.
         *
         * @return int the aligned size
         */
        public static function alignSize(int $size = 0, int $alignment = 0): int { }

        /**
         * Create a buffer and allocate the memory.
         *
         * Use new Buffer(0) when the size is not known. Then call alloc().
         */
        public function __construct(int $size) { }

        /**
         * Allocate the memory.
         *
         * The method works only on a buffer without memory. Use new Buffer(0).
         *
         * @throws \Error when the buffer already has memory
         */
        public function alloc(int $size): void { }

        public function getSize(): int { }

        public function getLength(): int { }

        public function getAvailableSize(): int { }

        public function isAvailable(): bool { }

        public function isEmpty(): bool { }

        public function isFull(): bool { }

        public function realloc(int $size): void { }

        /**
         * Grow the buffer.
         *
         * The buffer doubles its size until it reaches the recommended size. The result can be larger than the request.
         *
         * @param int $recommendSize [optional] = $this->getSize() * 2
         * @throws \ValueError when the size is not larger than the current size
         */
        public function extend(int $recommendSize = 0): void { }

        public function prepare(int $appendLength): void { }

        /**
         * Free the unused memory.
         *
         * The buffer shrinks to the used length.
         */
        public function mallocTrim(): void { }

        /**
         * read at max `$length` bytes data from start
         *
         * @throws \ValueError when specified `$start` and `$length` not in range
         * @phpstan-param int<0, max> $start
         * @psalm-param int<0, max> $start
         * @param int $start where to start reading
         * @phpstan-param int<-1, max> $length
         * @psalm-param int<-1, max> $length
         * @param int $length -1 meaning read all available, otherwise max length in byte to read
         * @return string data read
         */
        public function read(int $start = 0, int $length = -1): string { }

        /**
         * write `$string` to buffer
         *
         * `$start` and `$length` is for input `$string`
         * `$offset` is for output `$buffer`
         *
         * The offset must not be larger than the current length. The first write into a new buffer starts at 0.
         * The buffer grows when the data does not fit.
         *
         * @throws \ValueError when specified `$start`, `$offset` or `$length` not in range
         * @param string $string data to write
         * @phpstan-param int<0, max> $start
         * @psalm-param int<0, max> $start
         * @param int $start where to start copying data from string
         * @phpstan-param int<0, max> $offset
         * @psalm-param int<0, max> $offset
         * @param int $offset where to write data to buffer
         * @phpstan-param int<-1, max> $length
         * @psalm-param int<-1, max> $length
         * @param int $length meaning write all available data from string to buffer, otherwise max length in byte to write
         * @return int bytes written
         */
        public function write(int $offset, \Stringable|string $string, int $start = 0, int $length = -1): int { }

        /**
         * append `$string` to the end of buffer
         *
         * `$start` and `$length` is for input `$string`
         * The buffer grows when the data does not fit.
         *
         * @throws \ValueError when specified `$start`, `$offset` or `$length` not in range
         * @param string $string data to write
         * @phpstan-param int<0, max> $start
         * @psalm-param int<0, max> $start
         * @param int $start where to start copying data from string
         * @phpstan-param int<-1, max> $length
         * @psalm-param int<-1, max> $length
         * @param int $length -1 meaning write all available data from string to buffer, otherwise max length in byte to write
         * @return int bytes written
         */
        public function append(\Stringable|string $string, int $start = 0, int $length = -1): int { }

        /** @return int length of buffer after truncate */
        public function truncate(int $length): int { }

        /**
         * @param int $offset [optional] = $this->getOffset()
         * @return int length of buffer after truncate
         */
        public function truncateFrom(int $offset = 0, int $length = -1): int { }

        public function clear(): void { }

        /**
         * Get the value and reset the buffer.
         *
         * @return string the value
         * @throws \Error when the buffer is locked
         */
        public function fetchString(): string { }

        public function dupString(): string { }

        public function toString(): string { }

        /** @return bool true when the buffer is locked */
        public function isLocked(): bool { }

        /** @return Coroutine|null the locker, or null */
        public function getLocker(): Coroutine { }

        /**
         * Try to lock the buffer.
         *
         * @return bool false when the buffer is locked
         */
        public function tryLock(): bool { }

        /**
         * Lock the buffer.
         *
         * A locked buffer rejects write operations with Error. The lock holder is not exempt.
         * The lock stops write calls only. It is not a mutex.
         *
         * @throws \Error when the buffer is already locked
         */
        public function lock(): void { }

        /**
         * Unlock the buffer.
         *
         * @throws \Error when the buffer is not locked, or when the caller is not the locker
         */
        public function unlock(): void { }

        /**
         * Free the memory and set size and length to 0.
         *
         * A second close() does nothing. A write after close() returns the byte count and discards the data.
         */
        public function close(): void { }

        public function __toString(): string { }

        /** @return array<string, mixed> debug information for var_dump */
        public function __debugInfo(): array { }

        /**
         * Serialize the value.
         *
         * The size is not kept. After unserialize() the size equals the length.
         *
         * @return array{value: string}
         */
        public function __serialize(): array { }

        /** @param array<mixed, mixed>|array<mixed> $data */
        public function __unserialize(array $data): void { }
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

        public function __construct(int $type) { }

        /**
         * Use an existing socket stream.
         *
         * Use a socket of type TYPE_TCP. It has no native socket before bind or connect.
         *
         * @param resource $socket the stream to use
         * @throws SocketException when the stream is not a socket
         * @throws \Error when the socket already has a native socket
         */
        public function open(mixed $socket): static { }

        /**
         * @FIXME may cause error on 32-bit env
         * @return int 64-bit auto-incrementing ID, never repeat in the same process
         */
        public function getId(): int { }

        public function getType(): int { }

        public function getSimpleType(): int { }

        public function getTypeName(): string { }

        public function getSimpleTypeName(): string { }

        /** @return int real file descriptor number */
        public function getFd(): int { }

        public function getDnsTimeout(): int { }

        public function getAcceptTimeout(): int { }

        public function getConnectTimeout(): int { }

        public function getHandshakeTimeout(): int { }

        public function getReadTimeout(): int { }

        public function getWriteTimeout(): int { }

        /**
         * Set the timeouts.
         *
         * The method does not change the accept timeout. Use setAcceptTimeout() for it.
         * A negative value becomes -1.
         */
        public function setTimeout(int $timeout): static { }

        public function setDnsTimeout(int $timeout): static { }

        public function setAcceptTimeout(int $timeout): static { }

        public function setConnectTimeout(int $timeout): static { }

        public function setHandshakeTimeout(int $timeout): static { }

        public function setReadTimeout(int $timeout): static { }

        public function setWriteTimeout(int $timeout): static { }

        /** @param int $flags self::BIND_FLAG_* constants combinations */
        public function bind(string $name, int $port = 0, int $flags = self::BIND_FLAG_NONE): static { }

        public function listen(int $backlog = self::DEFAULT_BACKLOG): static { }

        /**
         * @param int $timeout [optional] = $this->getAcceptTimeout()
         * @return static Notice: it returns a new un-constructed connection instead of returning itself
         */
        public function accept(?int $timeout = null): static { }

        /**
         * @template T of static
         * @param T $connection
         * @param int $timeout [optional] = $this->getAcceptTimeout()
         * @return T Notice: it does not return the connection, but returns itself
         */
        public function acceptTo(self $connection, ?int $timeout = null): static { }

        /** @param int $timeout [optional] = $this->getConnectTimeout() */
        public function connect(string $name, int $port = 0, ?int $timeout = null): static { }

        /**
         * enable crypto (TLS) on socket
         * the options is a little different from PHP options,
         * PHP uses `cafile`, `local_cert`, `local_pk` while we use `ca_file`, `certificate`, `certificate_key`.
         * most other options is the same as PHP options.
         *
         * @note context switching may happen here
         *
         * @throws SocketException when options are invalid
         * @throws SocketException when IO error
         * @param array{
         *  'verify_peer'?: bool,
         *  'verify_peer_name'?: bool,
         *  'allow_self_signed'?: bool,
         *  'verify_depth'?: int,
         *  'ca_file'?: string,
         *  'ca_path'?: string,
         *  'security_level'?: int,
         *  'alpn_protocols'?: string,
         *  'passphrase'?: string,
         *  'certificate'?: string,
         *  'certificate_key'?: string,
         *  'no_ticket'?: bool,
         *  'no_compression'?: bool,
         *  'peer_name'?: string,
         *  'peer_fingerprint'?: string|array<string, string>,
         *  'SNI_server_certs'?: array<string, string>|array<string, array{
         *    'certificate'?: callable,
         *    'certificate_key'?: callable
         *   }>
         * } $options [optional]
         * The method ignores unknown options. verify_peer=true requires peer_name.
         *
         * @return static it returns itself
         */
        public function enableCrypto(?array $options = null): static { }

        public function getSockAddress(): string { }

        public function getSockPort(): int { }

        public function getPeerAddress(): string { }

        public function getPeerPort(): int { }

        /**
         * read `$length` bytes data into buffer from socket,
         * keep reading until `$length` bytes data received.
         *
         * The current length of the buffer must be at least the offset. A new buffer starts at 0.
         *
         * @note context switching may happen here
         *
         * @throws SocketException when got eof without enough data received
         * @throws SocketException when timed out
         * @throws SocketException when socket read failed
         * @param Buffer $buffer buffer to write in, data will be written from offset to offset + length
         * @phan-param int<0, max> $offset
         * @psalm-param int<0, max> $offset
         * @param int $offset offset is the start position to write in buffer
         * @phan-param int<-1, max> $length
         * @psalm-param int<-1, max> $length
         * @param int $length -1 meaning not limited, receive until eof, otherwise length in bytes
         * @param int|null $timeout timeout in milliseconds or null for using {@see Socket::getReadTimeout()} value
         * @return int bytes received
         */
        public function read(\Swow\Buffer $buffer, int $offset = 0, int $length = -1, ?int $timeout = null): int { }

        /**
         * read at max `$size` bytes data into buffer from socket,
         * returns when any data received or eof.
         *
         * @note context switching may happen here
         *
         * @throws SocketException when timed out
         * @throws SocketException when socket read failed
         * @param Buffer $buffer buffer to write in, data will be written from offset to offset + length
         * @phan-param int<0, max> $offset
         * @psalm-param int<0, max> $offset
         * @param int $offset offset is the start position to write in buffer
         * @phan-param int<-1, max> $size
         * @psalm-param int<-1, max> $size
         * @param int $size -1 meaning not limited, otherwise buffer size in bytes. only `$size` bytes data will be read from socket if there are more data than `$size` bytes, extra data will be kept in socket for further reading options
         * @param int|null $timeout timeout in milliseconds or null for using {@see Socket::getReadTimeout()} value
         * @return int bytes received
         */
        public function recv(\Swow\Buffer $buffer, int $offset = 0, int $size = -1, ?int $timeout = null): int { }

        /**
         * read at max `$size` bytes data into buffer from socket,
         * return when any data received.
         *
         * @note context switching may happen here
         *
         * @throws SocketException when no data received
         * @throws SocketException when timed out
         * @throws SocketException when socket read failed
         * @param Buffer $buffer buffer to write in, data will be written from offset to offset + length
         * @phan-param int<0, max> $offset
         * @psalm-param int<0, max> $offset
         * @param int $offset offset is the start position to write in buffer
         * @phan-param int<-1, max> $size
         * @psalm-param int<-1, max> $size
         * @param int $size -1 meaning not limited, otherwise buffer size in bytes. only `$size` bytes data will be read from socket if there are more data than `$size` bytes, extra data will be kept in socket for further reading options
         * @param int|null $timeout timeout in milliseconds or null for using {@see Socket::getReadTimeout()} value
         */
        public function recvData(\Swow\Buffer $buffer, int $offset = 0, int $size = -1, ?int $timeout = null): int { }

        /**
         * read at max `$size` bytes data into buffer from socket,
         * peer info is stored into `$address` and `$port` if applicable,
         * return when any data received or eof.
         *
         * @note context switching may happen here
         *
         * @throws SocketException when timed out
         * @throws SocketException when socket read failed
         * @param Buffer $buffer buffer to write in, data will be written from offset to offset + length
         * @phan-param int<0, max> $offset
         * @psalm-param int<0, max> $offset
         * @param int $offset offset is the start position to write in buffer
         * @phan-param int<-1, max> $size
         * @psalm-param int<-1, max> $size
         * @param int $size -1 meaning not limited, otherwise buffer size in bytes. only `$size` bytes data will be read from socket if there are more data than `$size` bytes, extra data will be kept in socket for further reading options
         * @param-out string &$address peer address in string
         * @phpstan-param int<0,65535>|null &$port
         * @psalm-param int<0,65535>|null &$port
         * @param-out int &$port peer address
         * @param int|null $timeout timeout in milliseconds or null for using {@see Socket::getReadTimeout()} value
         * @return int bytes received
         */
        public function recvFrom(\Swow\Buffer $buffer, int $offset = 0, int $size = -1, &$address = null, &$port = null, ?int $timeout = null): int { }

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
         * @param Buffer $buffer buffer to write in, data will be written from offset to offset + length
         * @phan-param int<0, max> $offset
         * @psalm-param int<0, max> $offset
         * @param int $offset offset is the start position to write in buffer
         * @phan-param int<-1, max> $size
         * @psalm-param int<-1, max> $size
         * @param int $size -1 meaning not limited, otherwise buffer size in bytes. only `$size` bytes data will be read from socket if there are more data than `$size` bytes, extra data will be kept in socket for further reading options
         * @param-out string &$address peer address in string
         * @phpstan-param int<0,65535>|null &$port
         * @psalm-param int<0,65535>|null &$port
         * @param-out int &$port peer address
         * @param int|null $timeout timeout in milliseconds or null for using {@see Socket::getReadTimeout()} value
         * @return int bytes received
         */
        public function recvDataFrom(\Swow\Buffer $buffer, int $offset = 0, int $size = -1, &$address = null, &$port = null, ?int $timeout = null): int { }

        /**
         * read at max `$size` bytes data into buffer from socket without removing the read data from socket,
         * only works on some type of socket,
         * returns immediately.
         *
         * @throws SocketException when socket read failed
         * @param Buffer $buffer buffer to write in, data will be written from offset to offset + length
         * @phan-param int<0, max> $offset
         * @psalm-param int<0, max> $offset
         * @param int $offset offset is the start position to write in buffer
         * @phan-param int<-1, max> $size
         * @psalm-param int<-1, max> $size
         * @param int $size -1 meaning not limited, otherwise buffer size in bytes.
         * @return int bytes received, 0 when no data is ready at the timeout
         */
        public function peek(\Swow\Buffer $buffer, int $offset = 0, int $size = -1, ?int $timeout = 0): int { }

        /**
         * read at max `$size` bytes data into buffer from socket without removing the read data from socket,
         * only works on some type of socket,
         * peer info is stored into `$address` and `$port` if applicable,
         * returns immediately.
         *
         * @throws SocketException when socket read failed
         * @param Buffer $buffer buffer to write in, data will be written from offset to offset + length
         * @phan-param int<0, max> $offset
         * @psalm-param int<0, max> $offset
         * @param int $offset offset is the start position to write in buffer
         * @phan-param int<-1, max> $size
         * @psalm-param int<-1, max> $size
         * @param int $size -1 meaning not limited, otherwise buffer size in bytes.
         * @param-out string &$address peer address in string
         * @phpstan-param int<0,65535>|null &$port
         * @psalm-param int<0,65535>|null &$port
         * @param-out int &$port peer address
         * @return int bytes received
         */
        public function peekFrom(\Swow\Buffer $buffer, int $offset = 0, int $size = -1, &$address = null, &$port = null, ?int $timeout = 0): int { }

        /**
         * read `$length` bytes data from socket as string,
         * keep reading until `$length` bytes data received.
         *
         * @note context switching may happen here
         *
         * @throws SocketException when got eof without enough data received
         * @throws SocketException when timed out
         * @throws SocketException when socket read failed
         * @phan-param int<1, max> $length
         * @psalm-param int<1, max> $length
         * @param int $length number of bytes, must be larger than 0
         * @param int|null $timeout timeout in milliseconds or null for using {@see Socket::getReadTimeout()} value
         * @return string data received in string
         * @throws \ValueError when the length is not larger than 0
         */
        public function readString(int $length = \Swow\Buffer::COMMON_SIZE, ?int $timeout = null): string { }

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
         * @param int|null $timeout timeout in milliseconds or null for using {@see Socket::getReadTimeout()} value
         * @return string data received in string
         */
        public function recvString(int $size = \Swow\Buffer::COMMON_SIZE, ?int $timeout = null): string { }

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
         * @param int|null $timeout timeout in milliseconds or null for using {@see Socket::getReadTimeout()} value
         * @return string data received in string
         */
        public function recvStringData(int $size = \Swow\Buffer::COMMON_SIZE, ?int $timeout = null): string { }

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
         * @param-out string &$address peer address in string
         * @phpstan-param int<0,65535>|null &$port
         * @psalm-param int<0,65535>|null &$port
         * @param-out int &$port peer address
         * @param int|null $timeout timeout in milliseconds or null for using {@see Socket::getReadTimeout()} value
         * @return string data received in string
         */
        public function recvStringFrom(int $size = \Swow\Buffer::COMMON_SIZE, &$address = null, &$port = null, ?int $timeout = null): string { }

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
         * @param-out string &$address peer address in string
         * @phpstan-param int<0,65535>|null &$port
         * @psalm-param int<0,65535>|null &$port
         * @param-out int &$port peer address
         * @param int|null $timeout timeout in milliseconds or null for using {@see Socket::getReadTimeout()} value
         * @return string data received in string
         */
        public function recvStringDataFrom(int $size = \Swow\Buffer::COMMON_SIZE, &$address = null, &$port = null, ?int $timeout = null): string { }

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
        public function peekString(int $size = \Swow\Buffer::COMMON_SIZE, ?int $timeout = 0): string { }

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
         * @param-out string &$address peer address in string
         * @phpstan-param int<0,65535>|null &$port
         * @psalm-param int<0,65535>|null &$port
         * @param-out int &$port peer address
         * @return string data received in string
         */
        public function peekStringFrom(int $size = \Swow\Buffer::COMMON_SIZE, &$address = null, &$port = null, ?int $timeout = 0): string { }

        /**
         * write io vector to socket
         *
         * vector is an array for all contents to write, elements in it maybe
         * - `Buffer`: write full text from start `$start`
         * - `string|Stringable`: write full text from start `$start`
         * - `array[Buffer $content, int $start, int $length]`: write it from start `$start`, with length `$length`
         * - `array[string|Stringable $content, int $start, int $length]`: write it from start `$start`, with length `$length`
         *
         * A null element ends the vector.
         * A write to a closed peer can succeed. Check getConnectionError() after the write.
         *
         * for code
         * ```php
         * $s = new Socket(Socket::TYPE_STDOUT);
         * $buffer = new Buffer(0);
         * $buffer->append('xyz');
         * $s->write([
         *     'abcd',
         *     ['abcdef', 2],
         *     ['abcdef', 1, 3],
         *     [$buffer, 1],
         * ]);
         * ```
         * string `"abcdcdefbcdyz"` will be written to the socket.
         *
         * @note context switching may happen here
         *
         * @throws SocketException when timed out
         * @throws SocketException when write failed
         * @throws \ValueError when an array element has more than 3 items
         * @phan-param non-empty-array<string|\Stringable|Buffer|array{0: string|\Stringable|Buffer, 1?: int, 2?: int}|null> $vector
         * @phpstan-param non-empty-array<string|\Stringable|Buffer|array{0: string|\Stringable|Buffer, 1?: int, 2?: int}|null> $vector
         * @psalm-param non-empty-array<string|\Stringable|Buffer|array{0: string|\Stringable|Buffer, 1?: int, 2?: int}|null> $vector
         * @param array<string|\Stringable|Buffer|array|null> $vector
         * @param int|null $timeout timeout in milliseconds or null for using {@see Socket::getWriteTimeout()} value
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
         * @phan-param non-empty-array<string|\Stringable|Buffer|array{0: string|\Stringable|Buffer, 1?: int, 2?: int}|null> $vector
         * @phpstan-param non-empty-array<string|\Stringable|Buffer|array{0: string|\Stringable|Buffer, 1?: int, 2?: int}|null> $vector
         * @psalm-param non-empty-array<string|\Stringable|Buffer|array{0: string|\Stringable|Buffer, 1?: int, 2?: int}|null> $vector
         * @param array<string|\Stringable|Buffer|array|null> $vector
         * @param string|null $address address to send to, may be ip or domain or path (for UNIX/UDG type)
         * @phpstan-param int<0, 65535>|null $port
         * @psalm-param int<0, 65535>|null $port
         * @param int|null $port port to send to
         * @param int|null $timeout timeout in milliseconds or null for using {@see Socket::getWriteTimeout()} value
         */
        public function writeTo(array $vector, ?string $address = null, ?int $port = null, ?int $timeout = null): static { }

        /**
         * write data into socket from `$start` of data with `$length` bytes
         *
         * A write to a closed peer can succeed. Check getConnectionError() after the write.
         *
         * @throws SocketException when timed out
         * @throws SocketException when write failed
         * @phpstan-param int<0, max> $start
         * @psalm-param int<0, max> $start
         * @param int $start where to start reading from $data
         * @phpstan-param int<-1, max> $length
         * @psalm-param int<-1, max> $length
         * @param int $length length of data to be sent, -1 meaning remaining data in $data, otherwise length in bytes.
         * @param int|null $timeout timeout in milliseconds or null for using {@see Socket::getWriteTimeout()} value
         */
        public function send(\Stringable|string $data, int $start = 0, int $length = -1, ?int $timeout = null): static { }

        /**
         * write data to specified address (and port if protocol needs port) from `$start` of data with `$length` bytes
         *
         * @throws SocketException when timed out
         * @throws SocketException when write failed
         * @phpstan-param int<0, max> $start
         * @psalm-param int<0, max> $start
         * @param int $start where to start reading from $data
         * @phpstan-param int<-1, max> $length
         * @psalm-param int<-1, max> $length
         * @param int $length length of data to be sent, -1 meaning remaining data in $data, otherwise length in bytes.
         * @param int|null $timeout timeout in milliseconds or null for using {@see Socket::getWriteTimeout()} value
         * @param string|null $address address to send to, may be ip or domain or path (for UNIX/UDG type)
         * @phpstan-param int<0, 65535>|null $port
         * @psalm-param int<0, 65535>|null $port
         * @param int|null $port port to send to
         */
        public function sendTo(\Stringable|string $data, int $start = 0, int $length = -1, ?string $address = null, ?int $port = null, ?int $timeout = null): static { }

        /**
         * Send a socket handle to the peer.
         *
         * The socket must be a named pipe with IPC enabled, type TYPE_IPCC.
         * The API has no method to receive the handle.
         *
         * @param int $timeout [optional] = $this->getWriteTimeout()
         * @throws SocketException when the socket is not an IPC pipe
         */
        public function sendHandle(self $handle, ?int $timeout = null): static { }

        /**
         * Send file without memory allocation or memory copy
         * @param int $offset [optional] = 0
         * @param int $timeout [optional] = $this->getWriteTimeout()
         */
        public function sendFile(string $filename, int $offset = 0, int $length = 0, ?int $timeout = null): int { }

        /** Recreate the native socket handle on this Socket object. */
        public function recreate(int $type): static { }

        public function close(): bool { }

        /** @return bool Whether the socket has been constructed and has not been closed */
        public function isAvailable(): bool { }

        /** @return bool Whether the socket has been opened with exact type and file descriptor is valid */
        public function isOpen(): bool { }

        /**
         * @return bool Whether the socket has connected (including SSL handshake if it's on SSL)
         * @note this method may return true even if the connection is broken, use {@see Socket::checkLiveness()} to check if the connection is broken
         */
        public function isEstablished(): bool { }

        public function isServer(): bool { }

        public function isServerConnection(): bool { }

        public function isClient(): bool { }

        /**
         * Get the connection error.
         *
         * @return int 0 when the connection is healthy, an Errno value when it is broken.
         * A socket without a descriptor gives EBADF (-9), an open socket without a connection gives ENOTCONN (-107).
         */
        public function getConnectionError(): int { }

        /** @throws SocketException when the socket connection is broken */
        public function checkLiveness(): static { }

        /** @return int self::IO_FLAG_* constants combinations */
        public function getIoState(): int { }

        public function getIoStateName(): string { }

        public function getIoStateNaming(): string { }

        public function getRecvBufferSize(): int { }

        public function getSendBufferSize(): int { }

        public function setRecvBufferSize(int $size): static { }

        public function setSendBufferSize(int $size): static { }

        public function setTcpNodelay(bool $enable): static { }

        public function setTcpKeepAlive(bool $enable, int $delay): static { }

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

        public static function supportsTcpReusePortLoadBalancing(): bool { }
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

        /**
         * Wait for the signal.
         *
         * One signal wakes all coroutines that wait for it. The handler exists only during the call.
         * Without a waiter the signal uses the default action.
         *
         * @note context switching may happen here
         *
         * @throws SignalException on timeout (-110) or failure
         */
        public static function wait(int $signal, int $timeout = -1): void { }

        /**
         * Send a signal to a process.
         *
         * @throws SignalException when the signal cannot be sent
         */
        public static function kill(int $pid, int $signal): void { }
    }
}

namespace Swow
{
    class SignalException extends \Swow\Exception { }
}

namespace Swow
{
    class Watchdog
    {
        /**
         * @param int $quantum Nanoseconds, if the blocking time exceeds the quantum, alerter will be triggered.
         * @param int $threshold Nanoseconds, if the blocking time exceeds the threshold (means the alerter does not alleviate the CPU starvation), it is considered that the system call blocking occurred.
         * @param callable|int|float|null $alerter
         * When it is callable, it will be called when blocking occurred, the developer can choose to suspend the coroutine or kill the coroutine;
         * when it is numeric, Coroutine will be delayed to run with sleep() in millisecond to alleviate CPU starvation;
         * when it is null, Coroutine will be delayed to run at the next round of the event loop starts to alleviate CPU starvation.
         *
         * The watchdog samples child coroutines only. A block in the main coroutine is not reported.
         * @return void
         */
        public static function run(int $quantum = 0, int $threshold = 0, callable|int|float|null $alerter = null): void { }

        public static function stop(): void { }

        public static function isRunning(): bool { }
    }
}

namespace Swow
{
    class WatchdogException extends \Swow\Exception { }
}

namespace Swow
{
    class IpAddress
    {
        public const IPV4 = 8;
        public const HAS_PORT = 1;
        public const HAS_MASK = 2;
        public const IPV4_EMBED = 4;

        /**
         * Parse an IP address.
         *
         * The address can have a port and a mask. The method does not resolve names and does not check the mask length.
         *
         * @throws IpAddressException when the address is invalid
         */
        public function __construct(string $address = '') { }

        public function getFlags(): int { }

        public function getFull(): string { }

        public function getIp(): string { }

        public function getPort(): int { }

        public function getMaskLen(): int { }

        public function setFlags(int $setFlags): static { }

        public function setFull(string $address): static { }

        public function setIp(string $address): static { }

        public function setPort(int $port): static { }

        public function setMaskLen(int $maskLen): static { }

        public function isMappedIpv4(): bool { }

        public function isIpv4OrMappedIpv4(): bool { }

        public function isLocal(): bool { }

        public function isLoopback(): bool { }

        public function in(self $range): bool { }

        public function covers(self $addr): bool { }

        public function convertToMappedIpv4(): static { }

        public function convertToIpv4(bool $force = false): static { }
    }
}

namespace Swow
{
    class IpAddressException extends \Swow\Exception { }
}

namespace Swow
{
    /**
     * get Number of PROCessors
     *
     * @param int $kind processors count kind, currently only support `Swow\NPROC_AVAILABLE` which means the number of available (i.e. online logical cores) processors
     * @return int
     * @throws \Swow\Exception when failed to get the number of processors
     */
    function nproc(int $kind = \SWOW\NPROC_AVAILABLE): int { }
}

namespace Swow
{
    /**
     * create a pipe pair like pipe(2)
     *
     * @param int $rflags read end flags, default is `0`
     * @param int $wflags write end flags, default is `0`
     * @return array{0: resource, 1: resource}|null the first element is the read end, the second element is the write end
     * @throws \Swow\Exception when failed to create pipe
     */
    function pipe(int $rflags = 0, int $wflags = 0): ?array { }
}

namespace Swow
{
    /**
     * get the file descriptor of a stream
     *
     * @param resource $stream the stream to get the file descriptor of, must be a PHP stream resource
     * @return int|null
     * @throws \Swow\Exception when failed to get the file descriptor
     */
    function fileno($stream): ?int { }
}

namespace Swow
{
    /**
     * create a pipe from a file descriptor
     *
     * The descriptor must be a pipe. The returned stream takes ownership of it.
     * When the stream closes, the descriptor closes too. The mode is not checked.
     *
     * @param int $fd the file descriptor to create a pipe from
     * @param string $mode the mode of the pipe, expected to be `r` or `w`
     * @return resource the pipe resource
     * @throws \Swow\Exception when the descriptor is not a pipe
     */
    function pipe_from_fd(int $fd, string $mode) { }
}

namespace Swow
{
    /**
     * Run a task when the current function frame ends.
     *
     * A return or an exception ends the frame. Tasks run in reverse order.
     * In the main scope the tasks run at request shutdown.
     */
    function defer(callable $tasks): void { }
}

namespace Swow\Utils
{
    final class Handler
    {
        protected function __construct() { }

        public function remove(): void { }
    }
}

namespace Swow\Sync
{
    class WaitReference
    {
        /**
         * Wait for the reference to be destroyed.
         *
         * @note context switching may happen here
         */
        public static function wait(self &$ref, int $timeout = -1): void { }

        public function __destruct() { }
    }
}

namespace Swow\Sync
{
    class WaitGroup
    {
        public function add(int $delta = 1): void { }

        /** @note context switching may happen here */
        public function wait(int $timeout = -1): void { }

        public function done(): void { }
    }
}

namespace Swow\Sync
{
    /**
     * A reusable barrier for a fixed number of OS threads.
     *
     * wait() blocks the whole OS thread, not only the current coroutine.
     * Use it only in a worker thread that runs one coroutine.
     * A cycle releases when count threads wait. The barrier works in one process only.
     */
    class ThreadBarrier
    {
        /**
         * Create a barrier.
         *
         * @param int $count number of threads in one cycle
         * @throws \ValueError when the count is out of range
         * @throws SyncException when the barrier is already constructed
         */
        public function __construct(int $count) { }

        /**
         * Wait for the other threads.
         *
         * @note this method blocks the whole OS thread
         *
         * @param int $timeout timeout in milliseconds, -1 waits forever
         * @return bool false on timeout
         * @throws SyncException when the caller cannot wait
         */
        public function wait(int $timeout = -1): bool { }

        /** Return the number of threads in one cycle. */
        public function getCount(): int { }

        /** Return the number of threads currently waiting. */
        public function getWaiting(): int { }

        /** Serialize the barrier by shared object id. */
        public function __serialize(): array { }

        /** Restore the barrier from a shared object id. */
        public function __unserialize(array $data): void { }

        /** Return debug information. */
        public function __debugInfo(): array { }
    }
}

namespace Swow\Sync
{
    /** @note context switching may happen here */
    function waitAll(int $timeout = -1): void { }
}

namespace Swow\Http
{
    class Http
    {
        public const DEFAULT_PROTOCOL_VERSION = '1.1';

        /**
         * Build a request string.
         *
         * The method adds no headers. An array header value writes one line per item.
         *
         * @return string the request
         */
        public static function packRequest(string $method, \Stringable|string $uri, array $headers = [], \Stringable|string $body = '', string $protocolVersion = self::DEFAULT_PROTOCOL_VERSION): string { }

        /**
         * Build a response string.
         *
         * An empty reason phrase uses the standard phrase. The status code is not checked.
         *
         * @return string the response
         */
        public static function packResponse(int $statusCode, string $reasonPhrase = '', array $headers = [], \Stringable|string $body = '', string $protocolVersion = self::DEFAULT_PROTOCOL_VERSION): string { }
    }
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

        /**
         * Get the reason phrase of a status code.
         *
         * @return string the phrase, "UNKNOWN" for an unknown code
         */
        public static function getReasonPhraseOf(int $statusCode): string { }
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

        /**
         * Parse data until the next event.
         *
         * Each call stops at one event. A begin or complete event consumes 0 bytes.
         * Call the method again with a larger start to continue.
         *
         * @return int bytes consumed by this call
         * @throws ParserException when the parse fails
         */
        public function execute(\Stringable|string $data, int $start = 0, int $length = -1): int { }

        public function getEvent(): int { }

        public function getEventName(): string { }

        public function getPreviousEvent(): int { }

        public function getPreviousEventName(): string { }

        /**
         * Get the offset of the current data.
         *
         * @return int the offset. Valid for data events only: URL, HEADER_FIELD, HEADER_VALUE, BODY.
         */
        public function getDataOffset(): int { }

        /** @return int the length of the current data */
        public function getDataLength(): int { }

        /**
         * Get the bytes that the last execute() call consumed.
         *
         * @return int the byte count of the last call, not cumulative
         */
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

        /**
         * Check whether the parser is in a multipart body.
         *
         * The parser ignores a boundary shorter than 2 characters.
         *
         * @return bool true when the parser is in a multipart body
         */
        public function isMultipart(): bool { }

        public function isUpgrade(): bool { }

        /**
         * Finish the current message.
         *
         * Use the method only when the message is incomplete.
         *
         * @throws ParserException when the message is complete
         */
        public function finish(): static { }

        /**
         * Reset the parser state.
         */
        public function reset(): static { }

        public static function getEventNameFor(int $event): string { }
    }
}

namespace Swow\Http
{
    class ParserException extends \Swow\Exception { }
}

namespace Swow\WebSocket
{
    class WebSocket
    {
        public const VERSION = 13;
        public const SECRET_KEY_LENGTH = 16;
        public const SECRET_KEY_ENCODED_LENGTH = 24;
        public const GUID = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11';
        public const HEADER_MIN_SIZE = 2;
        public const HEADER_MAX_SIZE = 14;
        public const EXT16_PAYLOAD_LENGTH = 126;
        public const EXT64_PAYLOAD_LENGTH = 127;
        public const EXT8_MAX_LENGTH = 125;
        public const EXT16_MAX_LENGTH = 65535;
        public const MASKING_KEY_LENGTH = 4;
        public const EMPTY_MASKING_KEY = '';
        public const DEFAULT_MASKING_KEY = '258E';
        public const PING_FRAME = "\x89\x00";
        public const PONG_FRAME = "\x8a\x80\x00\x00\x00\x00";

        /**
         * Mask data.
         *
         * An empty key gives the input unchanged. The default key is empty.
         *
         * @param string $maskingKey 0 or 4 bytes
         * @return string the masked data
         * @throws \ValueError when the key has another length
         */
        public static function mask(\Stringable|string $data, int $start = 0, int $length = -1, string $maskingKey = '', int $index = 0): string { }

        /**
         * Unmask the buffer in place.
         *
         * @param string $maskingKey 0 or 4 bytes
         * @throws \ValueError when the key has another length
         */
        public static function unmask(\Swow\Buffer $data, int $start = 0, int $length = -1, string $maskingKey = '', int $index = 0): void { }
    }
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

        /**
         * Get the name of the opcode.
         *
         * @return string the name
         */
        public static function getNameOf(int $opcode): string { }
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

        /**
         * Get the description of a close code.
         *
         * @return string the description, "Unknown websocket status code" for an unknown code
         */
        public static function getDescriptionOf(int $code): string { }
    }
}

namespace Swow\WebSocket
{
    /**
     * A WebSocket frame header.
     *
     * The object is a Buffer that holds the header bytes.
     */
    class Header extends \Swow\Buffer
    {
        public function __construct(bool $fin = true, bool $rsv1 = false, bool $rsv2 = false, bool $rsv3 = false, int $opcode = \Swow\WebSocket\Opcode::TEXT, int $payloadLength = 0, string $maskingKey = '') { }

        public function getHeaderSize(): int { }

        public function getOpcode(): int { }

        /**
         * Set the opcode.
         *
         * The opcode uses 4 bits. Larger values are truncated.
         */
        public function setOpcode(int $opcode): static { }

        public function getFin(): bool { }

        public function setFin(bool $fin): static { }

        public function getRSV1(): bool { }

        public function setRSV1(bool $rsv1): static { }

        public function getRSV2(): bool { }

        public function setRSV2(bool $rsv2): static { }

        public function getRSV3(): bool { }

        public function setRSV3(bool $rsv3): static { }

        public function getPayloadLength(): int { }

        /**
         * Set the payload length.
         *
         * The method does not check the value.
         */
        public function setPayloadLength(int $payloadLength): static { }

        public function getMask(): bool { }

        public function getMaskingKey(): string { }

        /**
         * Set the masking key.
         *
         * An empty key clears the mask bit. A 4 byte key sets the mask bit.
         */
        public function setMaskingKey(string $maskingKey): static { }

        public function setPayloadInfo(int $payloadLength, string $maskingKey = ''): static { }

        /** @return array<string, mixed> debug information for var_dump */
        public function __debugInfo(): array { }
    }
}

namespace Swow\Process
{
    /**
     * POSIX: SIGINT
     * Windows: CRT SIGINT, mapped to CTRL_C_EVENT by the process API
     */
    const SIGINT = 2;

    /**
     * POSIX: SIGKILL
     * Windows: mapped to TerminateProcess
     */
    const SIGKILL = 9;

    /**
     * POSIX only
     */
    const SIGTERM = 15;

    /**
     * Windows only: CRT SIGBREAK, mapped to CTRL_BREAK_EVENT
     */
    const SIGBREAK = 21;

    /**
     * Windows only: console control event value
     */
    const CTRL_C_EVENT = 0;

    /**
     * Windows only: console control event value
     */
    const CTRL_BREAK_EVENT = 1;

    /**
     * Send a signal to an arbitrary process.
     *
     * POSIX uses the signal number. Windows supports 0 (existence check),
     * SIGKILL (TerminateProcess), SIGINT (CTRL_C_EVENT), and SIGBREAK
     * (CTRL_BREAK_EVENT). SIGTERM is not supported on Windows.
     *
     * @param int $pid process id
     * @param int $signal platform signal value
     * @throws ProcessException when the signal cannot be sent
     */
    function kill(int $pid, int $signal = \Swow\Process\SIGTERM): void { }
}

namespace Swow\Process
{
    interface ProcessInterface
    {
        /**
         * Get the process id.
         */
        public function getPid(): int;

        /**
         * Wait for the process to exit.
         *
         * @param int $timeout timeout in milliseconds, -1 means infinite
         * @throws ProcessException when waiting fails or times out
         */
        public function wait(int $timeout = -1): ProcessExitStatus;

        /**
         * Check whether the process has exited.
         */
        public function hasExited(): bool;

        /**
         * Send a signal to the process.
         *
         * Windows supports SIGINT, SIGBREAK, and SIGKILL. SIGTERM is not
         * supported on Windows.
         *
         * @throws ProcessException when the signal cannot be sent
         */
        public function kill(int $signal): void;
    }
}

namespace Swow\Process
{
    final class ChildProcess implements \Swow\Process\ProcessInterface
    {
        /**
         * Spawn a command.
         *
         * stdio defaults to inherit. A pipe returns a PHP stream; a file
         * redirects to the given path; a resource uses the existing stream.
         * Extra child file descriptors can be passed through `fds`, with
         * child fd keys in the range 3 to 256.
         *
         * @param non-empty-list<string> $command
         * @param array{
         *     cwd?: string,
         *     env?: array<string, string>,
         *     stdin?: 'inherit'|'pipe'|resource|array{0: 'file', 1: string, 2?: string},
         *     stdout?: 'inherit'|'pipe'|resource|array{0: 'file', 1: string, 2?: string},
         *     stderr?: 'inherit'|'pipe'|resource|array{0: 'file', 1: string, 2?: string},
         *     fds?: array<int, resource|int>,
         * } $options
         * @throws ProcessException when the command cannot be spawned
         */
        public static function spawn(array $command, array $options = []): self { }

        /**
         * Fork the current process and run a closure in the child.
         *
         * POSIX only.
         * The child exits with the return value of the closure when the value is an int, or with 0.
         * An exit() call inside the closure does not set the exit code.
         *
         * @throws ProcessException
         */
        public static function fork(\Closure $callback): self { }

        /**
         * Get the process id.
         */
        public function getPid(): int { }

        /**
         * Wait for the process to exit.
         *
         * @param int $timeout timeout in milliseconds, -1 means infinite
         * @throws ProcessException when waiting fails or times out
         */
        public function wait(int $timeout = -1): ProcessExitStatus { }

        /**
         * Check whether the process has exited.
         */
        public function hasExited(): bool { }

        /**
         * Get the parent side of the stdin pipe.
         *
         * @return resource|null null unless stdin is a pipe
         */
        public function getStdin() { }

        /**
         * Get the parent side of the stdout pipe.
         *
         * @return resource|null null unless stdout is a pipe
         */
        public function getStdout() { }

        /**
         * Get the parent side of the stderr pipe.
         *
         * @return resource|null null unless stderr is a pipe
         */
        public function getStderr() { }

        /**
         * Send a signal to the process.
         *
         * Windows supports SIGINT, SIGBREAK, and SIGKILL. SIGTERM is not
         * supported on Windows.
         *
         * @throws ProcessException when the signal cannot be sent
         */
        public function kill(int $signal = \Swow\Process\SIGTERM): void { }

        /**
         * Close the process object.
         *
         * This never kills the process. Closing a running process abandons
         * it and releases the process handle; the child continues to run.
         * A POSIX child may remain a zombie until the parent exits. Calling
         * close more than once is allowed. After close the object cannot be
         * used for getPid, wait, hasExited, or kill.
         *
         * @throws ProcessException when another coroutine is waiting
         */
        public function close(): void { }

        /**
         * Close the process object during destruction.
         *
         * @internal
         */
        public function __destruct() { }
    }
}

namespace Swow\Process
{
    /**
     * Exit status of a ChildProcess.
     */
    class ProcessExitStatus
    {
        /**
         * Get the exit code.
         *
         * @return int exit code on normal exit, -1 when signaled
         */
        public function getExitCode(): int { }

        /**
         * Get the terminating signal.
         *
         * @return int terminating signal, -1 on normal exit
         */
        public function getTermSignal(): int { }

        /**
         * Check whether the process exited normally.
         */
        public function isExited(): bool { }

        /**
         * Check whether the process was terminated by a signal.
         */
        public function isSignaled(): bool { }

        /**
         * Check whether the process is stopped.
         *
         * Spawned processes always return false. On POSIX fork children this
         * reflects a stopped wait status.
         */
        public function isStopped(): bool { }
    }
}

namespace Swow\Process
{
    /**
     * Process API exception.
     */
    class ProcessException extends \Swow\Exception { }
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
    /**
     * Block the current thread for testing syscall blocking detection
     *
     * This function uses native sleep to simulate a real syscall blocking scenario,
     * which will be detected by Watchdog if it's running.
     *
     * @param int $timeout The time to block in milliseconds (must be positive)
     */
    function block(int $timeout): void { }
}

namespace Swow\Debug
{
    /**
     * Register a handler for extended statements.
     *
     * The method needs the CLI option -e. With force=true it starts the debug sampler instead.
     *
     * @return \Swow\Utils\Handler handler for remove()
     * @throws \Error when -e is missing and force is false
     */
    function registerExtendedStatementHandler(callable $handler, bool $force = false): \Swow\Utils\Handler { }
}
