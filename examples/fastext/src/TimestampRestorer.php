<?php

/**
 * This file is part of Swow
 *
 * @link    https://github.com/swow/swow
 * @contact twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

namespace Swow\Examples\Fastext;

use FFI;
use Throwable;

use function extension_loaded;
use function function_exists;
use function strlen;

use const DIRECTORY_SEPARATOR;
use const PHP_OS_FAMILY;
use const STDERR;

final class TimestampRestorer
{
    private const FILE_WRITE_ATTRIBUTES = 0x00000100;
    private const FILE_SHARE_READ_WRITE_DELETE = 0x00000007;
    private const OPEN_EXISTING = 3;
    private const FILE_ATTRIBUTE_NORMAL = 0x00000080;
    private const FILE_FLAG_BACKUP_SEMANTICS = 0x02000000;
    private const WINDOWS_EPOCH_OFFSET = 11644473600;

    private static ?FFI $ffi = null;

    private static bool $initialized = false;

    private static bool $available = false;

    public static function initialize(): void
    {
        if (self::$initialized) {
            return;
        }
        self::$initialized = true;

        if (PHP_OS_FAMILY !== 'Windows') {
            self::$available = true;
            return;
        }

        if (!extension_loaded('FFI') || !class_exists(FFI::class)) {
            fwrite(STDERR, "Warning: FFI is not available; modification times will not be restored\n");
            return;
        }
        if (!function_exists('iconv')) {
            fwrite(STDERR, "Warning: iconv is not available; modification times will not be restored\n");
            return;
        }

        try {
            self::$ffi = FFI::cdef(
                <<<'C'
typedef struct {
    uint32_t dwLowDateTime;
    uint32_t dwHighDateTime;
} FILETIME;

int64_t CreateFileW(
    uint16_t *lpFileName,
    uint32_t dwDesiredAccess,
    uint32_t dwShareMode,
    void *lpSecurityAttributes,
    uint32_t dwCreationDisposition,
    uint32_t dwFlagsAndAttributes,
    void *hTemplateFile
);

int32_t SetFileTime(
    int64_t hFile,
    const FILETIME *lpCreationTime,
    const FILETIME *lpLastAccessTime,
    const FILETIME *lpLastWriteTime
);

int32_t CloseHandle(int64_t hObject);
C,
                'kernel32.dll',
            );
            self::$available = true;
        } catch (Throwable $e) {
            fwrite(STDERR, "Warning: unable to initialize FFI; modification times will not be restored\n");
        }
    }

    public static function restore(string $path, int $mtime): bool
    {
        self::initialize();

        if (PHP_OS_FAMILY !== 'Windows') {
            return touch($path, $mtime);
        }

        if (!self::$available || self::$ffi === null) {
            return false;
        }

        try {
            $wide = iconv('UTF-8', 'UTF-16LE', str_replace('/', DIRECTORY_SEPARATOR, $path));
            if ($wide === false) {
                return false;
            }
            $wide .= "\0\0";
            $name = self::$ffi->new('uint16_t[' . (int) (strlen($wide) / 2) . ']');
            FFI::memcpy($name, $wide, strlen($wide));

            $isDir = is_dir($path);
            $handle = self::$ffi->CreateFileW(
                $name,
                self::FILE_WRITE_ATTRIBUTES,
                self::FILE_SHARE_READ_WRITE_DELETE,
                null,
                self::OPEN_EXISTING,
                $isDir ? self::FILE_FLAG_BACKUP_SEMANTICS : self::FILE_ATTRIBUTE_NORMAL,
                null,
            );
            if ($handle === -1 || $handle === 0) {
                return false;
            }

            $time = ($mtime + self::WINDOWS_EPOCH_OFFSET) * 10000000;
            $fileTime = self::$ffi->new('FILETIME');
            $fileTime->dwLowDateTime = $time & 0xFFFFFFFF;
            $fileTime->dwHighDateTime = ($time >> 32) & 0xFFFFFFFF;
            $ok = self::$ffi->SetFileTime($handle, null, null, FFI::addr($fileTime));
            self::$ffi->CloseHandle($handle);

            return $ok !== 0;
        } catch (Throwable) {
            return false;
        }
    }
}
