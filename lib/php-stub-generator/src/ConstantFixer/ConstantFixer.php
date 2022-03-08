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

class ConstantFixer
{
    /** @var array<string, ConstantDefinition> */
    private array $constantDefinitions;

    /**
     * @phan-param array<callable(string, array<string, ConstantDefinition>): string> $modifiers
     * @phpstan-param array<callable(string, array<string, ConstantDefinition>): string> $modifiers
     * @psalm-param array<callable(string, array<string, ConstantDefinition>): string> $modifiers
     * @param array<callable> $modifiers
     * @phan-param callable():array<string, ConstantDefinition>|null $getConstantDefinitions
     * @phpstan-param callable():array<string, ConstantDefinition>|null $getConstantDefinitions
     * @psalm-param callable():array<string, ConstantDefinition>|null $getConstantDefinitions
     */
    public function __construct(
        private array $modifiers,
        ?callable $getConstantDefinitions = null,
    ) {
        if ($getConstantDefinitions === null) {
            $definitions = [];
            foreach (['x86_64', 'arm64', 'mips64', 'riscv64'] as $arch) {
                $fetcher = new LinuxConstantDefinitionsFetcher(arch: $arch);
                $definitions[] = $fetcher->fetch();
            }
            foreach (['x86_64', 'arm64'] as $arch) {
                $fetcher = new XNUConstantDefinitionsFetcher(arch: $arch);
                $definitions[] = $fetcher->fetch();
            }
            foreach (['x86_64', 'arm64'] as $arch) {
                $fetcher = new WindowsConstantDefinitionsFetcher(arch: $arch);
                $definitions[] = $fetcher->fetch();
            }

            $merger = new ConstantDefinitionsMerger($definitions);

            $this->constantDefinitions = $merger->merge();
        } else {
            $this->constantDefinitions = $getConstantDefinitions();
        }
    }

    public function fix(
        string $fileName,
        bool $dryRun = true,
    ): void {
        $content = file_get_contents($fileName);
        foreach ($this->modifiers as $modifier) {
            $content = $modifier($content, $this->constantDefinitions);
        }
        if ($dryRun) {
            echo $content;
            return;
        }
        file_put_contents($fileName, $content);
    }
}
