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

namespace Swow\StubUtils\ConstantFixer;

use Exception;
use function array_key_exists;

/**
 * this class generates constants using latest github.com/torvalds/linux codes
 */
class LinuxConstantDefinitionsFetcher extends ConstantDefinitionsFetcherAbstract
{
    public const SIGNAL_RE = '/#define\s+(?<name>SIG[A-Z0-9]+)\s+(?<value>\d+)/';
    public const SIGNAL_HEADER_URL = '/include/uapi/asm-generic/signal.h';
    public const SIGNAL_HEADER_URLS = [
        'arm64' => '/arch/arm64/include/uapi/asm/signal.h',
        'mips64' => '/arch/mips/include/uapi/asm/signal.h',
        'x86_64' => '/arch/x86/include/uapi/asm/signal.h',
        'riscv64' => null,
    ];

    public const ERRNO_RE = '/#define\s+(?<name>E[A-Z0-9]+)\s+(?<value>\d+)\s*\/\*\s*(?<comment>.+?)\s*\*\//';
    public const ERRNO_BASE_HEADER_URL = '/include/uapi/asm-generic/errno-base.h';
    public const ERRNO_HEADER_URL = '/include/uapi/asm-generic/errno.h';

    /**
     * default page size
     */
    private int $pageSize;

    public function __construct(
        private string $arch = 'x86_64',
        private string $baseUrl = 'https://raw.githubusercontent.com/torvalds/linux/master',
        ?int $pageSize = null,
    ) {
        if (!array_key_exists($arch, static::SIGNAL_HEADER_URLS)) {
            throw new Exception("arch {$arch} is not supported yet");
        }
        if ($pageSize !== null) {
            $this->pageSize = $pageSize;
        } else {
            $this->pageSize = match ($arch) {
                'arm64', 'x86_64', 'riscv64' => 4096,
                'mips64' => 16384,
                default => 4096,
            };
        }
    }

    public function fetch(): ConstantDefinitionMap
    {
        $ret = new ConstantDefinitionMap($this->arch, 'Linux');

        $ret['PAGE_SIZE'] = new ConstantDefinition(
            value: $this->pageSize,
        );

        // get errno_base.h
        $errnoBase = $this->httpGet($this->baseUrl . static::ERRNO_BASE_HEADER_URL);
        preg_match_all(static::ERRNO_RE, $errnoBase, $matches);
        foreach ($matches['name'] as $index => $name) {
            //printf("got %s = %d".PHP_EOL, $name, $matches['value'][$index]);
            $ret[$name] = new ConstantDefinition(
                value: (int) $matches['value'][$index],
                comment: $matches['comment'][$index],
            );
        }

        // get errno.h
        // this will override(/concatenate with) errnos in errno_base.h
        $errno = $this->httpGet($this->baseUrl . static::ERRNO_HEADER_URL);
        preg_match_all(static::ERRNO_RE, $errno, $matches);
        foreach ($matches['name'] as $index => $name) {
            //printf("got %s = %d".PHP_EOL, $name, $matches['value'][$index]);
            $ret[$name] = new ConstantDefinition(
                value: (int) $matches['value'][$index],
                comment: $matches['comment'][$index],
            );
        }

        // generate UV_EXXX
        UVConstantConverter::convert($ret);

        // get asm-generic/signal.h
        $signalGeneric = $this->httpGet($this->baseUrl . static::SIGNAL_HEADER_URL);
        preg_match_all(static::SIGNAL_RE, $signalGeneric, $matches);
        foreach ($matches['name'] as $index => $name) {
            //printf("got %s = %d".PHP_EOL, $name, $matches['value'][$index]);
            $ret[$name] = new ConstantDefinition(
                value: (int) $matches['value'][$index],
            );
        }

        // get asm/signal.h
        // this will override signals in asm-generic/signal.h
        if (static::SIGNAL_HEADER_URLS[$this->arch] === null) {
            return $ret;
        }
        $signalGeneric = $this->httpGet($this->baseUrl . static::SIGNAL_HEADER_URLS[$this->arch]);
        preg_match_all(static::SIGNAL_RE, $signalGeneric, $matches);
        foreach ($matches['name'] as $index => $name) {
            //printf("got %s = %d".PHP_EOL, $name, $matches['value'][$index]);
            $ret[$name] = new ConstantDefinition(
                value: (int) $matches['value'][$index],
            );
        }

        return $ret;
    }
}
