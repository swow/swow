<?php

declare(strict_types=1);

namespace Swow\Examples\Gemm\Support;

use FFI;
use RuntimeException;

use function array_slice;
use function basename;
use function count;
use function dirname;
use function explode;
use function extension_loaded;
use function file_get_contents;
use function glob;
use function ini_get;
use function intdiv;
use function is_file;
use function max;
use function range;
use function sort;
use function substr;
use function trim;
use function unpack;

use const PHP_OS_FAMILY;

final class CpuAffinity
{
    private const MASK_BYTES = 128;

    private const THREAD_AFFINITY_POLICY = 4;

    private static ?FFI $linuxFfi = null;

    private static ?FFI $darwinFfi = null;

    private static ?FFI $windowsFfi = null;

    /** @return list<int> one logical CPU per physical core */
    public static function physicalCpus(string $mapping = 'auto'): array
    {
        if ($mapping === 'auto') {
            $mapping = PHP_OS_FAMILY === 'Linux' ? 'sysfs' : 'compact';
        }

        return match (PHP_OS_FAMILY) {
            'Linux' => self::linuxCpus($mapping),
            'Darwin' => self::darwinCpus($mapping),
            'Windows' => self::windowsCpus($mapping),
            default => throw new RuntimeException('CPU topology discovery is not implemented for ' . PHP_OS_FAMILY),
        };
    }

    public static function isAvailable(): bool
    {
        if (!extension_loaded('FFI')) {
            return false;
        }
        $enabled = ini_get('ffi.enable');

        return $enabled === '1' || $enabled === 'true' || $enabled === 'on';
    }

    public static function pin(int $cpu): void
    {
        match (PHP_OS_FAMILY) {
            'Linux' => self::pinLinux($cpu),
            'Darwin' => self::pinDarwin($cpu),
            'Windows' => self::pinWindows($cpu),
            default => throw new RuntimeException('CPU pinning is not implemented for ' . PHP_OS_FAMILY),
        };
    }

    /** @return list<int> */
    private static function linuxCpus(string $mapping): array
    {
        $online = self::linuxOnlineCpus();
        if ($mapping === 'compact') {
            return array_slice($online, 0, intdiv(count($online), 2));
        }
        if ($mapping !== 'sysfs') {
            throw new RuntimeException("Unknown CPU mapping: {$mapping}");
        }

        $cpus = [];
        $seen = [];
        foreach (glob('/sys/devices/system/cpu/cpu[0-9]*/topology/thread_siblings_list') ?: [] as $path) {
            $cpuDirectory = dirname($path, 2);
            $cpu = (int) substr(basename($cpuDirectory), 3);
            $onlineFile = $cpuDirectory . '/online';
            if (is_file($onlineFile) && trim((string) file_get_contents($onlineFile)) === '0') {
                continue;
            }
            $siblings = trim((string) file_get_contents($path));
            if ($siblings === '' || isset($seen[$siblings])) {
                continue;
            }
            $seen[$siblings] = true;
            $cpus[] = (int) explode(',', $siblings)[0];
        }
        sort($cpus);

        return $cpus;
    }

    /** @return list<int> */
    private static function linuxOnlineCpus(): array
    {
        $cpus = [];
        foreach (glob('/sys/devices/system/cpu/cpu[0-9]*') ?: [] as $cpuDirectory) {
            $onlineFile = $cpuDirectory . '/online';
            if (is_file($onlineFile) && trim((string) file_get_contents($onlineFile)) === '0') {
                continue;
            }
            $cpus[] = (int) substr(basename($cpuDirectory), 3);
        }
        sort($cpus);

        return $cpus;
    }

    private static function pinLinux(int $cpu): void
    {
        $ffi = self::linuxFfi();
        $mask = $ffi->new('unsigned char[' . self::MASK_BYTES . ']');
        FFI::memset($mask, 0, self::MASK_BYTES);
        $mask[$cpu >> 3] = 1 << ($cpu & 7);
        if ($ffi->sched_setaffinity(0, self::MASK_BYTES, $mask) !== 0) {
            throw new RuntimeException("Unable to pin worker to CPU {$cpu}");
        }
    }

    private static function linuxFfi(): FFI
    {
        if (self::$linuxFfi !== null) {
            return self::$linuxFfi;
        }
        if (!extension_loaded('FFI')) {
            throw new RuntimeException('The FFI extension is required for CPU pinning');
        }

        return self::$linuxFfi = FFI::cdef(
            'int sched_setaffinity(int pid, unsigned long cpusetsize, const unsigned char *mask);',
            null,
        );
    }

    /** @return list<int> */
    private static function darwinCpus(string $mapping): array
    {
        if ($mapping === 'sysfs') {
            throw new RuntimeException('The sysfs CPU mapping is only available on Linux');
        }
        if ($mapping !== 'compact') {
            throw new RuntimeException("Unknown CPU mapping: {$mapping}");
        }

        $physical = self::darwinCount('hw.physicalcpu');
        if ($physical === null) {
            $logical = self::darwinCount('hw.logicalcpu') ?? 1;
            $physical = max(1, intdiv($logical, 2));
        }

        return range(0, $physical - 1);
    }

    private static function darwinCount(string $name): ?int
    {
        $ffi = self::darwinFfi();
        $value = $ffi->new('int[1]');
        $size = $ffi->new('unsigned long[1]');
        $size[0] = 4;
        if ($ffi->sysctlbyname($name, $value, $size, null, 0) !== 0) {
            return null;
        }

        return $value[0];
    }

    private static function pinDarwin(int $cpu): void
    {
        $ffi = self::darwinFfi();
        $info = $ffi->new('integer_t[1]');
        $info[0] = $cpu + 1;
        $thread = $ffi->mach_thread_self();
        if ($ffi->thread_policy_set($thread, self::THREAD_AFFINITY_POLICY, $info, 1) !== 0) {
            throw new RuntimeException("Unable to set affinity hint for CPU {$cpu}");
        }
    }

    private static function darwinFfi(): FFI
    {
        if (self::$darwinFfi !== null) {
            return self::$darwinFfi;
        }
        if (!extension_loaded('FFI')) {
            throw new RuntimeException('The FFI extension is required for CPU pinning');
        }

        return self::$darwinFfi = FFI::cdef(
            <<<'C'
typedef unsigned int mach_port_t;
typedef int kern_return_t;
typedef unsigned int integer_t;
int sysctlbyname(const char *name, void *oldp, unsigned long *oldlenp, void *newp, unsigned long newlen);
mach_port_t mach_thread_self(void);
kern_return_t thread_policy_set(mach_port_t thread, int flavor, integer_t *policy_info, unsigned int count);
C,
            null,
        );
    }

    /** @return list<int> */
    private static function windowsCpus(string $mapping): array
    {
        if ($mapping === 'sysfs') {
            throw new RuntimeException('The sysfs CPU mapping is only available on Linux');
        }
        if ($mapping !== 'compact') {
            throw new RuntimeException("Unknown CPU mapping: {$mapping}");
        }

        $ffi = self::windowsFfi();
        $size = $ffi->new('unsigned long[1]');
        if ($ffi->GetLogicalProcessorInformation(null, $size) === 0 && $size[0] > 0) {
            $buffer = $ffi->new('unsigned char[' . $size[0] . ']');
            $returned = $ffi->new('unsigned long[1]');
            $returned[0] = $size[0];
            if ($ffi->GetLogicalProcessorInformation($buffer, $returned) !== 0) {
                $raw = FFI::string($buffer, $returned[0]);
                $cpus = [];
                for ($offset = 0; $offset + 32 <= $returned[0]; $offset += 32) {
                    $relationship = unpack('V', substr($raw, $offset + 8, 4))[1];
                    if ($relationship !== 0) {
                        continue;
                    }
                    $mask = unpack('P', substr($raw, $offset, 8))[1];
                    for ($bit = 0; $bit < 64; $bit++) {
                        if (($mask >> $bit) & 1) {
                            $cpus[] = $bit;
                            break;
                        }
                    }
                }
                if ($cpus !== []) {
                    sort($cpus);

                    return $cpus;
                }
            }
        }

        throw new RuntimeException('Unable to query the Windows processor topology');
    }

    private static function pinWindows(int $cpu): void
    {
        if ($cpu >= 63) {
            throw new RuntimeException('CPU pinning above processor 62 is not supported');
        }
        $ffi = self::windowsFfi();
        $thread = $ffi->GetCurrentThread();
        if ($ffi->SetThreadAffinityMask($thread, 1 << $cpu) === 0) {
            throw new RuntimeException("Unable to pin worker to CPU {$cpu}");
        }
    }

    private static function windowsFfi(): FFI
    {
        if (self::$windowsFfi !== null) {
            return self::$windowsFfi;
        }
        if (!extension_loaded('FFI')) {
            throw new RuntimeException('The FFI extension is required for CPU pinning');
        }

        return self::$windowsFfi = FFI::cdef(
            <<<'C'
typedef void *HANDLE;
HANDLE GetCurrentThread(void);
unsigned long long SetThreadAffinityMask(HANDLE hThread, unsigned long long dwThreadAffinityMask);
int GetLogicalProcessorInformation(void *Buffer, unsigned long *ReturnedLength);
C,
            'kernel32.dll',
        );
    }
}
