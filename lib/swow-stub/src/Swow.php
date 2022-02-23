<?php
/** @noinspection PhpUnused, PhpInconsistentReturnPointsInspection, PhpMissingParentConstructorInspection, PhpReturnDocTypeMismatchInspection */

namespace
{
    class Swow
    {
        public static function isBuiltWith(string $lib): bool { }
    }
}

namespace
{
    function msleep(int $milli_seconds): int { }
}

namespace Swow
{
    const MAJOR_VERSION = 0;
    const MINOR_VERSION = 1;
    const RELEASE_VERSION = 0;
    const EXTRA_VERSION = '';
    const VERSION = '0.1.0';
    const VERSION_ID = 100;
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
        public const TYPES_BUILTIN = 15089663;
        public const TYPES_USR = -16777216;
        public const TYPES_ALL = -1687553;
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

        public function getExecutedFilename(int $level = 0): string { }

        public function getExecutedLineno(int $level = 0): int { }

        public function getExecutedFunctionName(int $level = 0): string { }

        public function getTrace(int $level = 0, int $limit = 0, int $options = \DEBUG_BACKTRACE_PROVIDE_OBJECT): array { }

        public function getTraceAsString(int $level = 0, int $limit = 0, int $options = \DEBUG_BACKTRACE_PROVIDE_OBJECT): string { }

        public function getTraceAsList(int $level = 0, int $limit = 0, int $options = \DEBUG_BACKTRACE_PROVIDE_OBJECT): array { }

        public function getTraceDepth(int $limit = 0): int { }

        public function getDefinedVars(int $level = 0): array { }

        public function setLocalVar(string $name, mixed $value, int $level = 0, bool $force = true): static { }

        public function eval(string $string, int $level = 0): mixed { }

        public function call(callable $callable): mixed { }

        public function throw(\Throwable $throwable): mixed { }

        public function kill(): void { }

        public static function killAll(): void { }

        public static function count(): int { }

        public static function get(int $id): ?static { }

        public static function getAll(): array { }

        public function __debugInfo(): array { }
    }
}

namespace Swow
{
    class Channel
    {
        public const OPCODE_PUSH = 0;
        public const OPCODE_POP = 1;

        public function __construct(int $capacity = 0) { }

        public function push(mixed $data, int $timeout = -1): static { }

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

        public function __debugInfo(): array { }
    }
}

namespace Swow
{
    class Buffer implements \Stringable
    {
        public const PAGE_SIZE = 16384;
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
         * @param int $offset
         * @param int $whence
         */
        public function seek($offset, $whence = \SEEK_SET): static { }

        /** @param int $length */
        public function read($length = -1): string { }

        /** @param int $length */
        public function peek($length = -1): string { }

        /** @var int $offset [optional] = $this->getOffset() */
        public function peekFrom(?int $offset = null, int $length = -1): string { }

        public function getContents(): string { }

        /** @param string $string */
        public function write($string, int $offset = 0, int $length = -1): static { }

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

        public function __debugInfo(): array { }
    }
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
        public const TYPE_UNIX = 33554561;
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

        /** @var int $timeout [optional] = $this->getReadTimeout() */
        public function read(\Swow\Buffer $buffer, int $length = -1, ?int $timeout = null): int { }

        /** @var int $timeout [optional] = $this->getReadTimeout() */
        public function recv(\Swow\Buffer $buffer, int $size = -1, ?int $timeout = null): int { }

        /** @var int $timeout [optional] = $this->getReadTimeout() */
        public function recvData(\Swow\Buffer $buffer, int $size = -1, ?int $timeout = null): int { }

        /**
         * @param string $address [optional]
         * @param int $port [optional]
         * @var int $timeout [optional] = $this->getReadTimeout()
         */
        public function recvFrom(\Swow\Buffer $buffer, int $size = -1, &$address = null, &$port = null, ?int $timeout = null): int { }

        /**
         * @param string $address [optional]
         * @param int $port [optional]
         * @var int $timeout [optional] = $this->getReadTimeout()
         */
        public function recvDataFrom(\Swow\Buffer $buffer, int $size = -1, &$address = null, &$port = null, ?int $timeout = null): int { }

        public function peek(\Swow\Buffer $buffer, int $size = -1): int { }

        /**
         * @param string $address [optional]
         * @param int $port [optional]
         */
        public function peekFrom(\Swow\Buffer $buffer, int $size = -1, &$address = null, &$port = null): int { }

        /** @var int $timeout [optional] = $this->getReadTimeout() */
        public function readString(int $length = \Swow\Buffer::DEFAULT_SIZE, ?int $timeout = null): string { }

        /** @var int $timeout [optional] = $this->getReadTimeout() */
        public function recvString(int $size = \Swow\Buffer::DEFAULT_SIZE, ?int $timeout = null): string { }

        /** @var int $timeout [optional] = $this->getReadTimeout() */
        public function recvStringData(int $size = \Swow\Buffer::DEFAULT_SIZE, ?int $timeout = null): string { }

        /**
         * @param string $address
         * @param int $port
         * @var int $timeout [optional] = $this->getReadTimeout()
         */
        public function recvStringFrom(int $size = \Swow\Buffer::DEFAULT_SIZE, &$address = null, &$port = null, ?int $timeout = null): string { }

        /**
         * @param string $address
         * @param int $port
         * @var int $timeout [optional] = $this->getReadTimeout()
         */
        public function recvStringDataFrom(int $size = \Swow\Buffer::DEFAULT_SIZE, &$address = null, &$port = null, ?int $timeout = null): string { }

        public function peekString(int $size = \Swow\Buffer::DEFAULT_SIZE): string { }

        /**
         * @param string $address
         * @param int $port
         */
        public function peekStringFrom(int $size = \Swow\Buffer::DEFAULT_SIZE, &$address = null, &$port = null): string { }

        /** @var int $timeout [optional] = $this->getWriteTimeout() */
        public function write(array $vector, ?int $timeout = null): static { }

        /** @var int $timeout [optional] = $this->getWriteTimeout() */
        public function writeTo(array $vector, ?string $address = null, ?int $port = null, ?int $timeout = null): static { }

        /** @var int $timeout [optional] = $this->getWriteTimeout() */
        public function send(\Swow\Buffer $buffer, int $length = -1, ?int $timeout = null): static { }

        /** @var int $timeout [optional] = $this->getWriteTimeout() */
        public function sendTo(\Swow\Buffer $buffer, int $length = -1, ?string $address = null, ?int $port = null, ?int $timeout = null): static { }

        /** @var int $timeout [optional] = $this->getWriteTimeout() */
        public function sendString(string $string, ?int $timeout = null, int $offset = 0, int $length = -1): static { }

        /** @var int $timeout [optional] = $this->getWriteTimeout() */
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
    class Signal
    {
        public const HUP = 1;
        public const INT = 2;
        public const QUIT = 3;
        public const ILL = 4;
        public const TRAP = 5;
        public const ABRT = 6;
        public const IOT = 6;
        public const BUS = 10;
        public const FPE = 8;
        public const KILL = 9;
        public const USR1 = 30;
        public const SEGV = 11;
        public const USR2 = 31;
        public const PIPE = 13;
        public const ALRM = 14;
        public const TERM = 15;
        public const CHLD = 20;
        public const CONT = 19;
        public const STOP = 17;
        public const TSTP = 18;
        public const TTIN = 21;
        public const TTOU = 22;
        public const URG = 16;
        public const XCPU = 24;
        public const XFSZ = 25;
        public const VTALRM = 26;
        public const PROF = 27;
        public const WINCH = 28;
        public const IO = 23;
        public const INFO = 29;
        public const SYS = 12;

        public static function wait(int $num, int $timeout = -1): void { }

        public static function kill(int $pid, int $signum): void { }
    }
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

namespace Swow\Coroutine
{
    class Exception extends \Swow\Exception { }
}

namespace Swow\Coroutine
{
    final class UnwindExit extends \Swow\Coroutine\Exception { }
}

namespace Swow\Channel
{
    class Exception extends \Swow\Exception { }
}

namespace Swow\Channel
{
    class Selector
    {
        public function push(\Swow\Channel $channel, mixed $data): static { }

        public function pop(\Swow\Channel $channel): static { }

        public function commit(int $timeout = -1): \Swow\Channel { }

        public function fetch(): mixed { }

        public function getLastOpcode(): int { }
    }
}

namespace Swow\Channel\Selector
{
    class Exception extends \Swow\CallException { }
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
    class Exception extends \Swow\Exception { }
}

namespace Swow\Sync
{
    function waitAll(int $timeout = -1): void { }
}

namespace Swow\Buffer
{
    class Exception extends \Swow\Exception { }
}

namespace Swow\Socket
{
    class Exception extends \Swow\CallException { }
}

namespace Swow\Signal
{
    class Exception extends \Swow\Exception { }
}

namespace Swow\WatchDog
{
    class Exception extends \Swow\Exception { }
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

        public static function getReasonPhrase(int $statusCode): string { }
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
    function packMessage(array $headers = [], string $body = ''): string { }
}

namespace Swow\Http
{
    function packRequest(string $method = '', string $url = '', array $headers = [], string $body = '', string $protocolVersion = ''): string { }
}

namespace Swow\Http
{
    function packResponse(int $statusCode = 0, array $headers = [], string $body = '', string $reasonPhrase = '', string $protocolVersion = ''): string { }
}

namespace Swow\Http\Parser
{
    class Exception extends \Swow\Exception { }
}

namespace Swow\WebSocket
{
    const VERSION = 13;
    const SECRET_KEY_LENGTH = 16;
    const SECRET_KEY_ENCODED_LENGTH = 24;
    const GUID = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11';
    const HEADER_LENGTH = 2;
    const EXT16_LENGTH = 126;
    const EXT64_LENGTH = 127;
    const EXT8_MAX_LENGTH = 125;
    const EXT16_MAX_LENGTH = 65535;
    const MASK_KEY_LENGTH = 4;
    const EMPTY_MASK_KEY = '';
    const HEADER_BUFFER_SIZE = 128;
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

        public static function getName(int $opcode): int { }
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

        public static function getDescription(int $code): int { }
    }
}

namespace Swow\WebSocket
{
    class Frame implements \Stringable
    {
        public const PING = '27890027';
        public const PONG = '278a0027';

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

        public function __debugInfo(): array { }
    }
}

namespace Swow\Errno
{
    const E2BIG = -7;
    const EACCES = -13;
    const EADDRINUSE = -48;
    const EADDRNOTAVAIL = -49;
    const EAFNOSUPPORT = -47;
    const EAGAIN = -35;
    const EAI_ADDRFAMILY = -3000;
    const EAI_AGAIN = -3001;
    const EAI_BADFLAGS = -3002;
    const EAI_BADHINTS = -3013;
    const EAI_CANCELED = -3003;
    const EAI_FAIL = -3004;
    const EAI_FAMILY = -3005;
    const EAI_MEMORY = -3006;
    const EAI_NODATA = -3007;
    const EAI_NONAME = -3008;
    const EAI_OVERFLOW = -3009;
    const EAI_PROTOCOL = -3014;
    const EAI_SERVICE = -3010;
    const EAI_SOCKTYPE = -3011;
    const EALREADY = -37;
    const EBADF = -9;
    const EBUSY = -16;
    const ECANCELED = -89;
    const ECHARSET = -4080;
    const ECONNABORTED = -53;
    const ECONNREFUSED = -61;
    const ECONNRESET = -54;
    const EDESTADDRREQ = -39;
    const EEXIST = -17;
    const EFAULT = -14;
    const EFBIG = -27;
    const EHOSTUNREACH = -65;
    const EINTR = -4;
    const EINVAL = -22;
    const EIO = -5;
    const EISCONN = -56;
    const EISDIR = -21;
    const ELOOP = -62;
    const EMFILE = -24;
    const EMSGSIZE = -40;
    const ENAMETOOLONG = -63;
    const ENETDOWN = -50;
    const ENETUNREACH = -51;
    const ENFILE = -23;
    const ENOBUFS = -55;
    const ENODEV = -19;
    const ENOENT = -2;
    const ENOMEM = -12;
    const ENONET = -4056;
    const ENOPROTOOPT = -42;
    const ENOSPC = -28;
    const ENOSYS = -78;
    const ENOTCONN = -57;
    const ENOTDIR = -20;
    const ENOTEMPTY = -66;
    const ENOTSOCK = -38;
    const ENOTSUP = -45;
    const EOVERFLOW = -84;
    const EPERM = -1;
    const EPIPE = -32;
    const EPROTO = -100;
    const EPROTONOSUPPORT = -43;
    const EPROTOTYPE = -41;
    const ERANGE = -34;
    const EROFS = -30;
    const ESHUTDOWN = -58;
    const ESPIPE = -29;
    const ESRCH = -3;
    const ETIMEDOUT = -60;
    const ETXTBSY = -26;
    const EXDEV = -18;
    const UNKNOWN = -4094;
    const EOF = -4095;
    const ENXIO = -6;
    const EMLINK = -31;
    const EHOSTDOWN = -64;
    const EREMOTEIO = -4030;
    const ENOTTY = -25;
    const EFTYPE = -79;
    const EILSEQ = -92;
    const ESOCKTNOSUPPORT = -44;
    const ESTALE = -70;
    const UNCODED = -9763;
    const EPREV = -9762;
    const EMISUSE = -9761;
    const EVALUE = -9760;
    const ELOCKED = -9759;
    const ECLOSING = -9758;
    const ECLOSED = -9757;
    const EDEADLK = -9756;
    const ESSL = -9755;
    const ENOCERT = -9754;
    const ECERT = -9753;
}

namespace Swow\Errno
{
    function strerror(int $error): string { }
}

namespace Swow\Debug
{
    function buildTraceAsString(array $trace): string { }
}

namespace Swow\Debug
{
    function registerExtendedStatementHandler(callable $handler): \Swow\Util\Handler { }
}
