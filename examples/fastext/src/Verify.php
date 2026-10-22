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

final class Verify
{
    public static function compare(string $source, string $dest): bool
    {
        $source = realpath($source);
        $dest = realpath($dest);
        if ($source === false || $dest === false) {
            throw new \RuntimeException('Source or destination directory does not exist');
        }

        $left = Scanner::scan($source);
        $right = Scanner::scan($dest);
        $errors = [];

        $leftByName = [];
        foreach ($left as $metadata) {
            $leftByName[$metadata->name] = $metadata;
        }
        $rightByName = [];
        foreach ($right as $metadata) {
            $rightByName[$metadata->name] = $metadata;
        }

        foreach (array_diff_key($leftByName, $rightByName) as $name => $_) {
            $errors[] = "Missing in destination: {$name}";
        }
        foreach (array_diff_key($rightByName, $leftByName) as $name => $_) {
            $errors[] = "Extra in destination: {$name}";
        }

        foreach (array_intersect_key($leftByName, $rightByName) as $name => $metadata) {
            $other = $rightByName[$name];
            if (($metadata->mode & Metadata::S_IFMT) !== ($other->mode & Metadata::S_IFMT)) {
                $errors[] = "Type mismatch: {$name}";
                continue;
            }
            if (($metadata->mode & 0777) !== ($other->mode & 0777)) {
                $errors[] = "Mode mismatch: {$name}";
            }
            if ($metadata->isSymlink()) {
                if ($metadata->linkTarget !== $other->linkTarget) {
                    $errors[] = "Symlink target mismatch: {$name}";
                }
                continue;
            }
            if ($metadata->isRegular()) {
                if ($metadata->size !== $other->size) {
                    $errors[] = "Size mismatch: {$name}";
                    continue;
                }
                if (hash_file('sha256', $source . '/' . $name) !== hash_file('sha256', $dest . '/' . $name)) {
                    $errors[] = "Content mismatch: {$name}";
                }
            }
        }

        $leftLinks = self::hardlinkGroups($left);
        $rightLinks = self::hardlinkGroups($right);
        if ($leftLinks !== $rightLinks) {
            $errors[] = 'Hardlink groups do not match';
        }

        if ($errors !== []) {
            foreach ($errors as $error) {
                fwrite(STDERR, $error . "\n");
            }

            return false;
        }

        echo 'Verified ' . count($left) . " entries\n";

        return true;
    }

    /**
     * @param list<Metadata> $entries
     * @return list<list<string>>
     */
    private static function hardlinkGroups(array $entries): array
    {
        $groups = [];
        foreach ($entries as $metadata) {
            if (!$metadata->isRegular() || $metadata->nlink < 2) {
                continue;
            }
            $groups[$metadata->dev . ':' . $metadata->ino][] = $metadata->name;
        }
        $result = [];
        foreach ($groups as $names) {
            if (count($names) < 2) {
                continue;
            }
            sort($names);
            $result[] = $names;
        }
        sort($result);

        return $result;
    }
}
