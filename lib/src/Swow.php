<?php
/** @noinspection PhpUnused, PhpInconsistentReturnPointsInspection, PhpMissingParentConstructorInspection */

namespace
{
    class Swow { }
}

namespace
{
    /**
     * @param int $milli_seconds [required]
     * @return mixed
     */
     function msleep(int $milli_seconds) { }
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
        public const TYPE_WATCH_DOG = 131072;
        public const TYPE_PROTOCOL = 262144;
        public const TYPE_SSL = 4194304;
        public const TYPE_TEST = 8388608;
        public const TYPE_USR1 = 16777216;
        public const TYPE_USR2 = 33554432;
        public const TYPE_USR3 = 67108864;
        public const TYPE_USR4 = 134217728;
        public const TYPE_USR5 = 268435456;
        public const TYPE_USR6 = 536870912;
        public const TYPE_USR7 = 1073741824;
        public const TYPE_USR8 = -2147483648;
        public const TYPES_BUILTIN = 12984319;
        public const TYPES_USR = -16777216;
        public const TYPES_ALL = -3792897;
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
         * @return int
         */
        public static function getTypes(): int { }

        /**
         * @param int $types [required]
         * @return void
         */
        public static function setTypes(int $types): void { }

        /**
         * @return int
         */
        public static function getModuleTypes(): int { }

        /**
         * @param int $moduleTypes [required]
         * @return void
         */
        public static function setModuleTypes(int $moduleTypes): void { }
    }
}

namespace Swow
{
    interface Uncatchable { }
}

namespace Swow
{
    class Exception extends \RuntimeException { }
}

namespace Swow
{
    class CallException extends \Swow\Exception
    {
        protected $returnValue;

        /**
         * @return mixed
         */
        final function getReturnValue() { }
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
        public const STATE_INIT = 0;
        public const STATE_READY = 1;
        public const STATE_RUNNING = 2;
        public const STATE_WAITING = 3;
        public const STATE_LOCKED = 4;
        public const STATE_FINISHED = 5;
        public const STATE_DEAD = 6;

        /**
         * @param callable $callable [required]
         * @param int $stackPageSize [optional] = 0
         * @param int $stackSize [optional] = 0
         */
        public function __construct(callable $callable, int $stackPageSize = 0, int $stackSize = 0) { }

        /**
         * @param callable $callable [required]
         * @param mixed ...$data
         * @return $this
         */
        public static function run(callable $callable, ...$data) { }

        /**
         * @param mixed ...$data
         * @return mixed
         */
        public function resume(...$data) { }

        /**
         * @param mixed $data [optional] = null
         * @return mixed
         */
        public static function yield($data = null) { }

        /**
         * @return int
         */
        public function getId(): int { }

        /**
         * @return $this
         */
        public static function getCurrent() { }

        /**
         * @return $this
         */
        public static function getMain() { }

        /**
         * @return $this
         */
        public function getPrevious() { }

        /**
         * @return int
         */
        public function getState(): int { }

        /**
         * @return string
         */
        public function getStateName(): string { }

        /**
         * @return int
         */
        public function getRound(): int { }

        /**
         * @return int
         */
        public function getCurrentRound(): int { }

        /**
         * @return int
         */
        public function getElapsed(): int { }

        /**
         * @return string
         */
        public function getElapsedAsString(): string { }

        /**
         * @return int
         */
        public function getExitStatus(): int { }

        /**
         * @return bool
         */
        public function isAvailable(): bool { }

        /**
         * @return bool
         */
        public function isAlive(): bool { }

        /**
         * @param int $level [optional] = 0
         * @return string
         */
        public function getExecutedFilename(int $level = 0): string { }

        /**
         * @param int $level [optional] = 0
         * @return int
         */
        public function getExecutedLineno(int $level = 0): int { }

        /**
         * @param int $level [optional] = 0
         * @return string
         */
        public function getExecutedFunctionName(int $level = 0): string { }

        /**
         * @param int $level [optional] = 0
         * @param int $limit [optional] = 0
         * @param int $options [optional] = \DEBUG_BACKTRACE_PROVIDE_OBJECT
         * @return array
         */
        public function getTrace(int $level = 0, int $limit = 0, int $options = \DEBUG_BACKTRACE_PROVIDE_OBJECT): array { }

        /**
         * @param int $level [optional] = 0
         * @param int $limit [optional] = 0
         * @param int $options [optional] = \DEBUG_BACKTRACE_PROVIDE_OBJECT
         * @return string
         */
        public function getTraceAsString(int $level = 0, int $limit = 0, int $options = \DEBUG_BACKTRACE_PROVIDE_OBJECT): string { }

        /**
         * @param int $level [optional] = 0
         * @param int $limit [optional] = 0
         * @param int $options [optional] = \DEBUG_BACKTRACE_PROVIDE_OBJECT
         * @return array
         */
        public function getTraceAsList(int $level = 0, int $limit = 0, int $options = \DEBUG_BACKTRACE_PROVIDE_OBJECT): array { }

        /**
         * @param int $level [optional] = 0
         * @return array
         */
        public function getDefinedVars(int $level = 0): array { }

        /**
         * @param string $name [optional]
         * @param mixed $value [optional]
         * @param int $level [optional] = 0
         * @param bool $force [optional] = true
         * @return $this
         */
        public function setLocalVar(string $name, $value, int $level = 0, bool $force = true) { }

        /**
         * @param string $string [optional]
         * @param int $level [optional] = 0
         * @return null|mixed
         */
        public function eval(string $string, int $level = 0) { }

        /**
         * @param callable $callable [optional]
         * @return null|mixed
         */
        public function call(callable $callable) { }

        /**
         * @param \Throwable $throwable [required]
         * @return mixed
         */
        public function throw(\Throwable $throwable) { }

        /**
         * @param string $message [optional] = null
         * @param int $code [optional] = null
         * @return mixed
         */
        public function term(string $message = null, int $code = null) { }

        /**
         * @param string $message [optional] = null
         * @param int $code [optional] = null
         * @return mixed
         */
        public function kill(string $message = null, int $code = null) { }

        /**
         * @return int
         */
        public static function count(): int { }

        /**
         * @param int $id [optional]
         * @return ?$this
         */
        public static function get(int $id) { }

        /**
         * @return \Swow\Coroutine[]
         */
        public static function getAll(): array { }

        /**
         * @return array
         */
        public function __debugInfo(): array { }
    }
}

namespace Swow
{
    class Channel
    {
        public const OPCODE_PUSH = 0;
        public const OPCODE_POP = 1;

        /**
         * @param int $capacity [optional] = 0
         */
        public function __construct(int $capacity = 0) { }

        /**
         * @param mixed $data [required]
         * @param int $timeout [optional] = -1
         * @return $this
         */
        public function push($data, int $timeout = -1) { }

        /**
         * @param int $timeout [optional] = -1
         * @return mixed
         */
        public function pop(int $timeout = -1) { }

        /**
         * @return mixed
         */
        public function close() { }

        /**
         * @return int
         */
        public function getCapacity(): int { }

        /**
         * @return int
         */
        public function getLength(): int { }

        /**
         * @return bool
         */
        public function isAvailable(): bool { }

        /**
         * @return bool
         */
        public function hasProducers(): bool { }

        /**
         * @return bool
         */
        public function hasConsumers(): bool { }

        /**
         * @return bool
         */
        public function isEmpty(): bool { }

        /**
         * @return bool
         */
        public function isFull(): bool { }

        /**
         * @return bool
         */
        public function isReadable(): bool { }

        /**
         * @return bool
         */
        public function isWritable(): bool { }

        /**
         * @return array
         */
        public function __debugInfo(): array { }
    }
}

namespace Swow
{
    class Event
    {
        /**
         * @return void
         */
        public static function wait(): void { }
    }
}

namespace Swow
{
    class Buffer
    {
        public const PAGE_SIZE = 4096;
        public const DEFAULT_SIZE = 8192;

        /**
         * @param int $size [optional] = 0
         * @param int $alignment [optional] = 0
         * @return int
         */
        public static function alignSize(int $size = 0, int $alignment = 0): int { }

        /**
         * @param int $size [optional] = \Swow\Buffer::DEFAULT_SIZE
         */
        public function __construct(int $size = \Swow\Buffer::DEFAULT_SIZE) { }

        /**
         * @param int $size [optional] = \Swow\Buffer::DEFAULT_SIZE
         * @return $this
         */
        public function alloc(int $size = \Swow\Buffer::DEFAULT_SIZE) { }

        /**
         * @return int
         */
        public function getSize(): int { }

        /**
         * @return int
         */
        public function getLength(): int { }

        /**
         * @return int
         */
        public function getAvailableSize(): int { }

        /**
         * @return int
         */
        public function getReadableLength(): int { }

        /**
         * @return int
         */
        public function getWritableSize(): int { }

        /**
         * @return bool
         */
        public function isReadable(): bool { }

        /**
         * @return bool
         */
        public function isWritable(): bool { }

        /**
         * @return bool
         */
        public function isSeekable(): bool { }

        /**
         * @return bool
         */
        public function isAvailable(): bool { }

        /**
         * @return bool
         */
        public function isEmpty(): bool { }

        /**
         * @return bool
         */
        public function isFull(): bool { }

        /**
         * @param int $newSize [optional] = 0
         * @return $this
         */
        public function realloc(int $newSize = 0) { }

        /**
         * @param int $recommendSize [optional] = $this->getSize() * 2
         * @return $this
         */
        public function extend(int $recommendSize = null) { }

        /**
         * @return $this
         */
        public function mallocTrim() { }

        /**
         * @return int
         */
        public function tell(): int { }

        /**
         * @return $this
         */
        public function rewind() { }

        /**
         * @return bool
         */
        public function eof(): bool { }

        /**
         * @param mixed $offset [required]
         * @param mixed $whence [optional] = \SEEK_SET
         * @return $this
         */
        public function seek($offset, $whence = \SEEK_SET) { }

        /**
         * @param mixed $length [optional] = 0
         * @return string
         */
        public function read($length = 0): string { }

        /**
         * @param mixed $length [optional] = 0
         * @return string
         */
        public function peek($length = 0): string { }

        /**
         * @param int $offset [optional] = $this->getOffset()
         * @param int $length [optional] = 0
         * @return string
         */
        public function peekFrom(int $offset = null, int $length = 0): string { }

        /**
         * @return string
         */
        public function getContents(): string { }

        /**
         * @param mixed $string [required]
         * @param int $offset [optional] = 0
         * @param int $length [optional] = 0
         * @return $this
         */
        public function write($string, int $offset = 0, int $length = 0) { }

        /**
         * @param int $start [optional] = $this->getOffset()
         * @param int $length [optional] = 0
         * @return $this
         */
        public function truncate(int $start = null, int $length = 0) { }

        /**
         * @return $this
         */
        public function clear() { }

        /**
         * @return string
         */
        public function fetchString(): string { }

        /**
         * @return string
         */
        public function dupString(): string { }

        /**
         * @return string
         */
        public function toString(): string { }

        /**
         * @return mixed
         */
        public function close() { }

        /**
         * @return string
         */
        public function __toString(): string { }

        /**
         * @return array
         */
        public function __debugInfo(): array { }
    }
}

namespace Swow
{
    class Socket
    {
        public const INVALID_FD = -1;
        public const DEFAULT_BACKLOG = 128;
        public const TYPE_FLAG_STREAM = 1;
        public const TYPE_FLAG_DGRAM = 2;
        public const TYPE_FLAG_INET = 16;
        public const TYPE_FLAG_IPV4 = 32;
        public const TYPE_FLAG_IPV6 = 64;
        public const TYPE_FLAG_LOCAL = 128;
        public const TYPE_FLAG_UNSPEC = 1024;
        public const TYPE_FLAG_TCP_DELAY = 2048;
        public const TYPE_FLAG_TCP_KEEPALIVE = 4096;
        public const TYPE_FLAG_UDP_BROADCAST = 2048;
        public const TYPE_FLAG_IPC = 1024;
        public const TYPE_FLAG_STDIN = 1024;
        public const TYPE_FLAG_STDOUT = 2048;
        public const TYPE_FLAG_STDERR = 4096;
        public const TYPE_FLAG_SERVER = 1048576;
        public const TYPE_FLAG_CLIENT = 2097152;
        public const TYPE_FLAG_SESSION = 4194304;
        public const TYPE_ANY = 0;
        public const TYPE_TCP = 16777233;
        public const TYPE_TCP4 = 16777265;
        public const TYPE_TCP6 = 16777297;
        public const TYPE_PIPE = 33554561;
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

        /**
         * @param int $type [optional] = \Swow\Socket::TYPE_TCP
         * @param int $fd [optional] = \Swow\Socket::INVALID_FD
         */
        public function __construct(int $type = \Swow\Socket::TYPE_TCP, int $fd = \Swow\Socket::INVALID_FD) { }

        /**
         * @return int
         */
        public function getType(): int { }

        /**
         * @return string
         */
        public function getTypeName(): string { }

        /**
         * @return int
         */
        public function getFd(): int { }

        /**
         * @return int
         */
        public function getDnsTimeout(): int { }

        /**
         * @return int
         */
        public function getAcceptTimeout(): int { }

        /**
         * @return int
         */
        public function getConnectTimeout(): int { }

        /**
         * @return int
         */
        public function getHandshakeTimeout(): int { }

        /**
         * @return int
         */
        public function getReadTimeout(): int { }

        /**
         * @return int
         */
        public function getWriteTimeout(): int { }

        /**
         * @param int $timeout [required]
         * @return $this
         */
        public function setTimeout(int $timeout) { }

        /**
         * @param int $timeout [required]
         * @return $this
         */
        public function setDnsTimeout(int $timeout) { }

        /**
         * @param int $timeout [required]
         * @return $this
         */
        public function setAcceptTimeout(int $timeout) { }

        /**
         * @param int $timeout [required]
         * @return $this
         */
        public function setConnectTimeout(int $timeout) { }

        /**
         * @param int $timeout [required]
         * @return $this
         */
        public function setHandshakeTimeout(int $timeout) { }

        /**
         * @param int $timeout [required]
         * @return $this
         */
        public function setReadTimeout(int $timeout) { }

        /**
         * @param int $timeout [required]
         * @return $this
         */
        public function setWriteTimeout(int $timeout) { }

        /**
         * @param string $name [required]
         * @param int $port [optional] = 0
         * @param int $flags [optional] = \Swow\Socket::BIND_FLAG_NONE
         * @return $this
         */
        public function bind(string $name, int $port = 0, int $flags = \Swow\Socket::BIND_FLAG_NONE) { }

        /**
         * @param int $backlog [optional] = \Swow\Socket::DEFAULT_BACKLOG
         * @return $this
         */
        public function listen(int $backlog = \Swow\Socket::DEFAULT_BACKLOG) { }

        /**
         * @param null|\Swow\Socket $object [optional] = $this
         * @param null|int $timeout [optional] = $this->getAcceptTimeout()
         * @return $this
         */
        public function accept(?\Swow\Socket $object = null, ?int $timeout = null) { }

        /**
         * @param string $name [required]
         * @param int $port [optional] = 0
         * @param null|int $timeout [optional] = $this->getConnectTimeout()
         * @return $this
         */
        public function connect(string $name, int $port = 0, ?int $timeout = null) { }

        /**
         * @return string
         */
        public function getSockAddress(): string { }

        /**
         * @return int
         */
        public function getSockPort(): int { }

        /**
         * @return string
         */
        public function getPeerAddress(): string { }

        /**
         * @return int
         */
        public function getPeerPort(): int { }

        /**
         * @param \Swow\Buffer $buffer [required]
         * @param null|int $length [optional] = $this->getWritableSize()
         * @param null|int $timeout [optional] = $this->getReadTimeout()
         * @return int
         */
        public function read(\Swow\Buffer $buffer, ?int $length = null, ?int $timeout = null): int { }

        /**
         * @param \Swow\Buffer $buffer [required]
         * @param null|int $size [optional] = $this->getWritableSize()
         * @param null|int $timeout [optional] = $this->getReadTimeout()
         * @return int
         */
        public function recv(\Swow\Buffer $buffer, ?int $size = null, ?int $timeout = null): int { }

        /**
         * @param \Swow\Buffer $buffer [required]
         * @param null|int $size [optional] = $this->getWritableSize()
         * @param null|int $timeout [optional] = $this->getReadTimeout()
         * @return int
         */
        public function recvData(\Swow\Buffer $buffer, ?int $size = null, ?int $timeout = null): int { }

        /**
         * @param \Swow\Buffer $buffer [required]
         * @param null|int $size [optional] = $this->getWritableSize()
         * @param mixed $address [optional] = null
         * @param mixed $port [optional] = null
         * @param null|int $timeout [optional] = $this->getReadTimeout()
         * @return int
         */
        public function recvFrom(\Swow\Buffer $buffer, ?int $size = null, &$address = null, &$port = null, ?int $timeout = null): int { }

        /**
         * @param \Swow\Buffer $buffer [required]
         * @param null|int $size [optional] = $this->getWritableSize()
         * @param mixed $address [optional] = null
         * @param mixed $port [optional] = null
         * @param null|int $timeout [optional] = $this->getReadTimeout()
         * @return int
         */
        public function recvDataFrom(\Swow\Buffer $buffer, ?int $size = null, &$address = null, &$port = null, ?int $timeout = null): int { }

        /**
         * @param \Swow\Buffer $buffer [required]
         * @param null|int $size [optional] = $this->getWritableSize()
         * @return int
         */
        public function peek(\Swow\Buffer $buffer, ?int $size = null): int { }

        /**
         * @param \Swow\Buffer $buffer [required]
         * @param null|int $size [optional] = $this->getWritableSize()
         * @param mixed $address [optional] = null
         * @param mixed $port [optional] = null
         * @return int
         */
        public function peekFrom(\Swow\Buffer $buffer, ?int $size = null, &$address = null, &$port = null): int { }

        /**
         * @param null|int $length [required] = $this->getWritableSize()
         * @param null|int $timeout [optional] = $this->getReadTimeout()
         * @return string
         */
        public function readString(?int $length = null, ?int $timeout = null): string { }

        /**
         * @param null|int $size [optional] = $this->getWritableSize()
         * @param null|int $timeout [optional] = $this->getReadTimeout()
         * @return string
         */
        public function recvString(?int $size = null, ?int $timeout = null): string { }

        /**
         * @param null|int $size [optional] = $this->getWritableSize()
         * @param null|int $timeout [optional] = $this->getReadTimeout()
         * @return string
         */
        public function recvStringData(?int $size = null, ?int $timeout = null): string { }

        /**
         * @param null|int $size [optional] = $this->getWritableSize()
         * @param mixed $address [optional] = null
         * @param mixed $port [optional] = null
         * @param null|int $timeout [optional] = $this->getReadTimeout()
         * @return string
         */
        public function recvStringFrom(?int $size = null, &$address = null, &$port = null, ?int $timeout = null): string { }

        /**
         * @param null|int $size [optional] = $this->getWritableSize()
         * @param mixed $address [optional] = null
         * @param mixed $port [optional] = null
         * @param null|int $timeout [optional] = $this->getReadTimeout()
         * @return string
         */
        public function recvStringDataFrom(?int $size = null, &$address = null, &$port = null, ?int $timeout = null): string { }

        /**
         * @param null|int $size [optional] = $this->getWritableSize()
         * @return string
         */
        public function peekString(?int $size = null): string { }

        /**
         * @param null|int $size [optional] = $this->getWritableSize()
         * @param mixed $address [optional] = null
         * @param mixed $port [optional] = null
         * @return string
         */
        public function peekStringFrom(?int $size = null, &$address = null, &$port = null): string { }

        /**
         * @param array $vector [required]
         * @param null|int $timeout [optional] = $this->getWriteTimeout()
         * @return $this
         */
        public function write(array $vector, ?int $timeout = null) { }

        /**
         * @param array $vector [required]
         * @param mixed $address [optional] = null
         * @param mixed $port [optional] = null
         * @param null|int $timeout [optional] = $this->getWriteTimeout()
         * @return $this
         */
        public function writeTo(array $vector, $address = null, $port = null, ?int $timeout = null) { }

        /**
         * @param \Swow\Buffer $buffer [required]
         * @param null|int $length [optional] = $this->getReadableLength()
         * @param null|int $timeout [optional] = $this->getWriteTimeout()
         * @return $this
         */
        public function send(\Swow\Buffer $buffer, ?int $length = null, ?int $timeout = null) { }

        /**
         * @param \Swow\Buffer $buffer [required]
         * @param null|int $length [optional] = $this->getReadableLength()
         * @param mixed $address [optional] = null
         * @param mixed $port [optional] = null
         * @param null|int $timeout [optional] = $this->getWriteTimeout()
         * @return $this
         */
        public function sendTo(\Swow\Buffer $buffer, ?int $length = null, $address = null, $port = null, ?int $timeout = null) { }

        /**
         * @param string $string [required]
         * @param null|int $timeout [optional] = $this->getWriteTimeout()
         * @param int $offset [optional] = 0
         * @param int $length [optional] = 0
         * @return $this
         */
        public function sendString(string $string, ?int $timeout = null, int $offset = 0, int $length = 0) { }

        /**
         * @param string $string [required]
         * @param mixed $address [optional] = null
         * @param mixed $port [optional] = null
         * @param null|int $timeout [optional] = $this->getWriteTimeout()
         * @param int $offset [optional] = 0
         * @param int $length [optional] = 0
         * @return $this
         */
        public function sendStringTo(string $string, $address = null, $port = null, ?int $timeout = null, int $offset = 0, int $length = 0) { }

        /**
         * @return bool
         */
        public function close(): bool { }

        /**
         * @return bool
         */
        public function isAvailable(): bool { }

        /**
         * @return bool
         */
        public function isEstablished(): bool { }

        /**
         * @return $this
         */
        public function checkLiveness() { }

        /**
         * @return int
         */
        public function getIoState(): int { }

        /**
         * @return string
         */
        public function getIoStateName(): string { }

        /**
         * @return string
         */
        public function getIoStateNaming(): string { }

        /**
         * @return int
         */
        public function getRecvBufferSize(): int { }

        /**
         * @return int
         */
        public function getSendBufferSize(): int { }

        /**
         * @param int $size [required]
         * @return $this
         */
        public function setRecvBufferSize(int $size) { }

        /**
         * @param int $size [required]
         * @return $this
         */
        public function setSendBufferSize(int $size) { }

        /**
         * @param bool $enable [optional]
         * @return $this
         */
        public function setTcpNodelay(bool $enable) { }

        /**
         * @param bool $enable [optional]
         * @param int $delay [optional]
         * @return $this
         */
        public function setTcpKeepAlive(bool $enable, int $delay) { }

        /**
         * @param bool $enable [optional]
         * @return $this
         */
        public function setTcpAcceptBalance(bool $enable) { }

        /**
         * @return array
         */
        public function __debugInfo(): array { }

        /**
         * @param int $timeout [required]
         * @return mixed
         */
        public static function setGlobalTimeout(int $timeout) { }

        /**
         * @return int
         */
        public static function getGlobalDnsTimeout(): int { }

        /**
         * @return int
         */
        public static function getGlobalAcceptTimeout(): int { }

        /**
         * @return int
         */
        public static function getGlobalConnectTimeout(): int { }

        /**
         * @return int
         */
        public static function getGlobalHandshakeTimeout(): int { }

        /**
         * @return int
         */
        public static function getGlobalReadTimeout(): int { }

        /**
         * @return int
         */
        public static function getGlobalWriteTimeout(): int { }

        /**
         * @param int $timeout [required]
         * @return mixed
         */
        public static function setGlobalDnsTimeout(int $timeout) { }

        /**
         * @param int $timeout [required]
         * @return mixed
         */
        public static function setGlobalAcceptTimeout(int $timeout) { }

        /**
         * @param int $timeout [required]
         * @return mixed
         */
        public static function setGlobalConnectTimeout(int $timeout) { }

        /**
         * @param int $timeout [required]
         * @return mixed
         */
        public static function setGlobalHandshakeTimeout(int $timeout) { }

        /**
         * @param int $timeout [required]
         * @return mixed
         */
        public static function setGlobalReadTimeout(int $timeout) { }

        /**
         * @param int $timeout [required]
         * @return mixed
         */
        public static function setGlobalWriteTimeout(int $timeout) { }
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
        public const BUS = 7;
        public const FPE = 8;
        public const KILL = 9;
        public const USR1 = 10;
        public const SEGV = 11;
        public const USR2 = 12;
        public const PIPE = 13;
        public const ALRM = 14;
        public const TERM = 15;
        public const CHLD = 17;
        public const STKFLT = 16;
        public const CONT = 18;
        public const STOP = 19;
        public const TSTP = 20;
        public const TTIN = 21;
        public const TTOU = 22;
        public const URG = 23;
        public const XCPU = 24;
        public const XFSZ = 25;
        public const VTALRM = 26;
        public const PROF = 27;
        public const WINCH = 28;
        public const IO = 29;
        public const POLL = 29;
        public const PWR = 30;
        public const SYS = 31;

        /**
         * @param int $num [required]
         * @param int $timeout [optional]
         * @return void
         */
        public static function wait(int $num, int $timeout): void { }
    }
}

namespace Swow
{
    class WatchDog
    {
        /**
         * @param int $quantum [optional] = 0
         * @param int $threshold [optional] = 0
         * @param callable|int|float|null $alerter [optional] = null
         * @return void
         */
        public static function run(int $quantum = 0, int $threshold = 0, $alerter = null): void { }

        /**
         * @return void
         */
        public static function stop(): void { }

        /**
         * @return bool
         */
        public static function isRunning(): bool { }
    }
}

namespace Swow
{
    /**
     * @param callable $tasks [optional]
     * @return void
     */
     function defer(callable $tasks): void { }
}

namespace Swow\Coroutine
{
    class Exception extends \Swow\Exception { }
}

namespace Swow\Coroutine
{
    class CrossException extends \Swow\Coroutine\Exception
    {
        protected $coroutine;

        /**
         * @return mixed
         */
        final function getCoroutine() { }
    }
}

namespace Swow\Coroutine
{
    class TermException extends \Swow\Coroutine\CrossException { }
}

namespace Swow\Coroutine
{
    class KillException extends \Swow\Coroutine\CrossException implements \Swow\Uncatchable { }
}

namespace Swow\Channel
{
    class Exception extends \Swow\Exception { }
}

namespace Swow\Channel
{
    class Selector
    {
        /**
         * @param \Swow\Channel $channel [required]
         * @param mixed $data [required]
         * @return $this
         */
        public function push(\Swow\Channel $channel, $data) { }

        /**
         * @param \Swow\Channel $channel [required]
         * @return $this
         */
        public function pop(\Swow\Channel $channel) { }

        /**
         * @param int $timeout [optional] = -1
         * @return \Swow\Channel
         */
        public function commit(int $timeout = -1): \Swow\Channel { }

        /**
         * @return mixed
         */
        public function fetch() { }

        /**
         * @return int
         */
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
        /**
         * @param \Swow\Sync\WaitReference $waitReference [required]
         * @param int $timeout [optional] = -1
         * @return void
         */
        public static function wait(\Swow\Sync\WaitReference &$waitReference, int $timeout = -1): void { }

        public function __destruct() { }
    }
}

namespace Swow\Sync
{
    class WaitGroup
    {
        /**
         * @param int $delta [optional] = 1
         * @return void
         */
        public function add(int $delta = 1): void { }

        /**
         * @param int $timeout [optional]
         * @return void
         */
        public function wait(int $timeout): void { }

        /**
         * @return void
         */
        public function done(): void { }
    }
}

namespace Swow\Sync
{
    class Exception extends \Swow\Exception { }
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

        /**
         * @param int $statusCode [required]
         * @return string
         */
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
        public const EVENT_MESSAGE_BEGIN = 65536;
        public const EVENT_URL = 131078;
        public const EVENT_STATUS = 262150;
        public const EVENT_HEADER_FIELD = 524292;
        public const EVENT_HEADER_VALUE = 1048580;
        public const EVENT_HEADERS_COMPLETE = 2097152;
        public const EVENT_BODY = 4194308;
        public const EVENT_MESSAGE_COMPLETE = 8388608;
        public const EVENT_CHUNK_HEADER = 16777216;
        public const EVENT_CHUNK_COMPLETE = 33554432;
        public const EVENTS_NONE = 0;
        public const EVENTS_ALL = 67043334;

        /**
         * @return int
         */
        public function getType(): int { }

        /**
         * @param int $type [required]
         * @return $this
         */
        public function setType(int $type) { }

        /**
         * @return int
         */
        public function getEvents(): int { }

        /**
         * @param int $events [required]
         * @return $this
         */
        public function setEvents(int $events) { }

        /**
         * @param \Swow\Buffer $buffer [required]
         * @param mixed $data [optional] = null
         * @return int
         */
        public function execute(\Swow\Buffer $buffer, &$data = null): int { }

        /**
         * @return int
         */
        public function getEvent(): int { }

        /**
         * @return string
         */
        public function getEventName(): string { }

        /**
         * @return int
         */
        public function getDataOffset(): int { }

        /**
         * @return int
         */
        public function getDataLength(): int { }

        /**
         * @return int
         */
        public function getParsedLength(): int { }

        /**
         * @return bool
         */
        public function isCompleted(): bool { }

        /**
         * @return bool
         */
        public function shouldKeepAlive(): bool { }

        /**
         * @return string
         */
        public function getMethod(): string { }

        /**
         * @return int
         */
        public function getMajorVersion(): int { }

        /**
         * @return int
         */
        public function getMinorVersion(): int { }

        /**
         * @return string
         */
        public function getProtocolVersion(): string { }

        /**
         * @return int
         */
        public function getStatusCode(): int { }

        /**
         * @return string
         */
        public function getReasonPhrase(): string { }

        /**
         * @return int
         */
        public function getContentLength(): int { }

        /**
         * @return bool
         */
        public function isUpgrade(): bool { }

        /**
         * @return $this
         */
        public function finish() { }

        /**
         * @return $this
         */
        public function reset() { }
    }
}

namespace Swow\Http
{
    /**
     * @param array $headers [optional] = []
     * @param string $body [optional] = ''
     * @return string
     */
     function packMessage(array $headers = [], string $body = ''): string { }
}

namespace Swow\Http
{
    /**
     * @param string $method [optional] = ''
     * @param string $url [optional] = ''
     * @param array $headers [optional] = []
     * @param string $body [optional] = ''
     * @param string $protocolVersion [optional] = ''
     * @return string
     */
     function packRequest(string $method = '', string $url = '', array $headers = [], string $body = '', string $protocolVersion = ''): string { }
}

namespace Swow\Http
{
    /**
     * @param int $statusCode [optional] = 0
     * @param array $headers [optional] = []
     * @param string $body [optional] = ''
     * @param string $reasonPhrase [optional] = ''
     * @param string $protocolVersion [optional] = ''
     * @return string
     */
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

        /**
         * @param int $opcode [required]
         * @return int
         */
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

        /**
         * @param int $code [required]
         * @return int
         */
        public static function getDescription(int $code): int { }
    }
}

namespace Swow\WebSocket
{
    class Frame
    {
        public const PING = '27890027';
        public const PONG = '278a0027';

        /**
         * @param \Swow\Buffer $buffer [required]
         * @return int
         */
        public function unpackHeader(\Swow\Buffer $buffer): int { }

        /**
         * @return $this
         */
        public function resetHeader() { }

        /**
         * @return int
         */
        public function getOpcode(): int { }

        /**
         * @param int $opcode [required]
         * @return $this
         */
        public function setOpcode(int $opcode) { }

        /**
         * @return int
         */
        public function getFin(): int { }

        /**
         * @param int $fin [required]
         * @return $this
         */
        public function setFin(int $fin) { }

        /**
         * @return int
         */
        public function getRSV1(): int { }

        /**
         * @param int $rsv1 [required]
         * @return $this
         */
        public function setRSV1(int $rsv1) { }

        /**
         * @return int
         */
        public function getRSV2(): int { }

        /**
         * @param int $rsv2 [required]
         * @return $this
         */
        public function setRSV2(int $rsv2) { }

        /**
         * @return int
         */
        public function getRSV3(): int { }

        /**
         * @param int $rsv3 [required]
         * @return $this
         */
        public function setRSV3(int $rsv3) { }

        /**
         * @return int
         */
        public function getHeaderLength(): int { }

        /**
         * @return int
         */
        public function getPayloadLength(): int { }

        /**
         * @param int $payloadLength [required]
         * @return $this
         */
        public function setPayloadLength(int $payloadLength) { }

        /**
         * @return int
         */
        public function getMask(): int { }

        /**
         * @param bool $mask [required]
         * @return $this
         */
        public function setMask(bool $mask) { }

        /**
         * @return string
         */
        public function getMaskKey(): string { }

        /**
         * @param string $maskKey [required]
         * @return $this
         */
        public function setMaskKey(string $maskKey) { }

        /**
         * @return int
         */
        public function hasPayloadData(): int { }

        /**
         * @return \Swow\Buffer
         */
        public function getPayloadData(): \Swow\Buffer { }

        /**
         * @return string
         */
        public function getPayloadDataAsString(): string { }

        /**
         * @param null|\Swow\Buffer $buffer [required]
         * @return $this
         */
        public function setPayloadData(?\Swow\Buffer $buffer) { }

        /**
         * @return $this
         */
        public function unmaskPayloadData() { }

        /**
         * @param bool $headerOnly [optional] = false
         * @return string
         */
        public function toString(bool $headerOnly = false): string { }

        /**
         * @return string
         */
        public function __toString(): string { }

        /**
         * @return array
         */
        public function __debugInfo(): array { }
    }
}

namespace Swow\Errno
{
    const E2BIG = -7;
    const EACCES = -13;
    const EADDRINUSE = -98;
    const EADDRNOTAVAIL = -99;
    const EAFNOSUPPORT = -97;
    const EAGAIN = -11;
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
    const EALREADY = -114;
    const EBADF = -9;
    const EBUSY = -16;
    const ECANCELED = -125;
    const ECHARSET = -4080;
    const ECONNABORTED = -103;
    const ECONNREFUSED = -111;
    const ECONNRESET = -104;
    const EDESTADDRREQ = -89;
    const EEXIST = -17;
    const EFAULT = -14;
    const EFBIG = -27;
    const EHOSTUNREACH = -113;
    const EINTR = -4;
    const EINVAL = -22;
    const EIO = -5;
    const EISCONN = -106;
    const EISDIR = -21;
    const ELOOP = -40;
    const EMFILE = -24;
    const EMSGSIZE = -90;
    const ENAMETOOLONG = -36;
    const ENETDOWN = -100;
    const ENETUNREACH = -101;
    const ENFILE = -23;
    const ENOBUFS = -105;
    const ENODEV = -19;
    const ENOENT = -2;
    const ENOMEM = -12;
    const ENONET = -64;
    const ENOPROTOOPT = -92;
    const ENOSPC = -28;
    const ENOSYS = -38;
    const ENOTCONN = -107;
    const ENOTDIR = -20;
    const ENOTEMPTY = -39;
    const ENOTSOCK = -88;
    const ENOTSUP = -95;
    const EPERM = -1;
    const EPIPE = -32;
    const EPROTO = -71;
    const EPROTONOSUPPORT = -93;
    const EPROTOTYPE = -91;
    const ERANGE = -34;
    const EROFS = -30;
    const ESHUTDOWN = -108;
    const ESPIPE = -29;
    const ESRCH = -3;
    const ETIMEDOUT = -110;
    const ETXTBSY = -26;
    const EXDEV = -18;
    const UNKNOWN = -4094;
    const EOF = -4095;
    const ENXIO = -6;
    const EMLINK = -31;
    const EHOSTDOWN = -112;
    const EREMOTEIO = -121;
    const ENOTTY = -25;
    const EFTYPE = -4028;
    const EILSEQ = -84;
    const UNCODED = -9763;
    const EPREV = -9762;
    const EMISUSE = -9761;
    const EVALUE = -9760;
    const ELOCKED = -9759;
    const ECLOSING = -9758;
    const ECLOSED = -9757;
    const EDEADLK = -9756;
    const ESSL = -9755;
}

namespace Swow\Errno
{
    /**
     * @param int $error [optional]
     * @return string
     */
     function strerror(int $error): string { }
}

namespace Swow\Debug
{
    /**
     * @param array $trace [required]
     * @return string
     */
     function buildTraceAsString(array $trace): string { }
}

namespace Swow\Debug
{
    /**
     * @param null|callable $handler [required]
     * @return null|callable
     */
     function registerExtendedStatementHandler(?callable $handler): ?callable { }
}
