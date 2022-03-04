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

use function count;
use const SORT_REGULAR;

/**
 * this class merges constants
 */
class ConstantDefinitionsMerger
{
    /**
     * @param array<ConstantDefinitionMap> $constDefinitionMaps
     * @param string|int $reference
     */
    public function __construct(
        private array $constDefinitionMaps,
        private string|int $reference = 0,
    ) {
    }

    /**
     * merge constDefinitionMaps into one string->constdef map
     *
     * @return array<string,ConstantDefinition>
     */
    public function merge(): array
    {
        $couples = [];
        $osArchs = [];
        $container = [];
        $ret = [];
        $defaults = [];
        // merge arches
        foreach ($this->constDefinitionMaps as $i => $map) {
            /** @var ConstantDefinitionMap $map */
            $os = $map->getOS();
            $arch = $map->getArch();
            $couples[] = [$os, $arch];
            $osArchs[$os][] = [$os, $arch];

            foreach ($map as $name => $def) {
                /** @var ConstantDefinition $def */
                $container[$name][$os][$arch] = $def;
                if ($i === $this->reference) {
                    $defaults[$name] = $def->value;
                }
            }
        }

        foreach ($container as $name => $_os) {
            $value = $defaults[$name] ?? null;
            $comments = [];

            $remainingCouples = $couples;
            foreach ($_os as $os => $_arch) {
                foreach ($_arch as $arch => $def) {
                    /** @var ConstantDefinition $def */
                    // if default system donot have a value, use first arch occured instead
                    if ($value === null) {
                        $value = $def->value;
                    }
                    $hasValue = $def->value !== $value ? " may have a value `{$def->value}`" : '';
                    $meaning = $def->comment !== '' ? " means \"{$def->comment}\"" : '';
                    $comments["this constant{$hasValue}{$meaning}"][] = [$os, $arch];
                    $remainingCouples = array_filter($remainingCouples, static fn ($x) => !($x[0] === $os && $x[1] === $arch));
                }
            }
            foreach ($remainingCouples as $couple) {
                $comments['this constant may not exist'][] = $couple;
            }

            // generate final comment
            $comment = '';
            foreach ($comments as $c => $plats) {
                if ($c === 'this constant') {
                    // empty comment, skip it
                    continue;
                }

                $plats = array_unique($plats, SORT_REGULAR);
                $newPlats = [];
                foreach ($osArchs as $os => $osPlats) {
                    $a = array_map(static fn ($x) => $x[0] . $x[1], $osPlats);
                    $b = array_map(static fn ($x) => $x[0] . $x[1], $plats);
                    if (count(array_intersect($a, $b)) === count($a)) {
                        // all arches is the same
                        $newPlats[] = $os;
                        $plats = array_filter($plats, static fn ($x) => $x[0] !== $os);
                    }
                }
                $newPlats = [...$newPlats, ...array_map(static fn ($x) => $x[0] . ' ' . $x[1], $plats)];

                if (count($newPlats) === 1) {
                    $comment .= "\nAt " . $newPlats[0] . " platform, {$c}";
                    continue;
                }

                $lastPlat = array_pop($newPlats);
                $comment .= "\nAt " . implode(', ', $newPlats) . ' and ' . $lastPlat . " platforms, {$c}";
            }

            $ret[$name] = new ConstantDefinition(
                value: $value,
                comment: $comment
            );
            //printf('%s: %d===%s===' . PHP_EOL, $name, $ret[$name]->value, $ret[$name]->comment);
        }

        return $ret;
    }
}
