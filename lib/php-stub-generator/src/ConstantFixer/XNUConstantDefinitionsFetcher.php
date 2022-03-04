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

use function Swow\Util\httpGet;

/**
 * this class generates constants using latest github.com/apple/darwin-xnu codes
 */
class XNUConstantDefinitionsFetcher implements ConstantDefinitionsFetcher
{

    const SIGNAL_RE = '/#define\s+(?<name>SIG[A-Z]+)\s+(?<value>\d+)\s*\/\*\s*(?<comment>.+?)\s*\*\//';
    const SIGNAL_HEADER_URL = '/bsd/sys/signal.h';

    /**
     * default page size
     *
     * @var int $pageSize
     */
    private int $pageSize;
    public function __construct(
        private string $arch = 'x86_64',
        private string $baseUrl = 'https://raw.githubusercontent.com/apple/darwin-xnu/main',
        ?int $pageSize = null,
    ) {
        if ($pageSize !== null) {
            $this->pageSize = $pageSize;
        } else {
            $this->pageSize = match ($arch) {
                'x86_64' => 4096,
                'arm64' => 16384,
                default => 4096,
            };
        }
    }

    public function fetch(): ConstantDefinitionMap
    {
        $ret = new ConstantDefinitionMap($this->arch, 'macOS');

        $ret['PAGE_SIZE'] = new ConstantDefinition(
            value: $this->pageSize,
        );

        // get asm-generic/signal.h
        $signalGeneric = httpGet($this->baseUrl . static::SIGNAL_HEADER_URL);
        preg_match_all(static::SIGNAL_RE, $signalGeneric, $matches);
        foreach ($matches['name'] as $index => $name) {
            $comment = $matches['comment'][$index];
            //printf("got %s = %d %s" . PHP_EOL, $name, $matches['value'][$index], $matches['comment'][$index]);
            $ret[$name] = new ConstantDefinition(
                value: (int)$matches['value'][$index],
                comment: $comment,
            );
        }


        return $ret;
    }
}
