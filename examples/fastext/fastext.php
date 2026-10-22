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

require __DIR__ . '/autoload.php';

use Swow\Examples\Fastext\Fastext;
use Swow\Examples\Fastext\Verify;

function usage(): void
{
    fwrite(STDERR, <<<'TXT'
usage:
  fastext.php archive --source=<dir> --blob=<file> [--workers=N] [--mode=fork|inline|script] [--compression=auto|zstd|zlib|none]
  fastext.php extract --blob=<file> --dest=<dir> [--workers=N] [--mode=fork|inline|script]
  fastext.php verify --source=<dir> --dest=<dir>

TXT);
    exit(1);
}

$command = $argv[1] ?? '';
if ($command === '') {
    usage();
}

$options = [];
for ($i = 2; $i < $argc; $i++) {
    $arg = $argv[$i];
    if (!str_starts_with($arg, '--')) {
        usage();
    }
    if (str_contains($arg, '=')) {
        [$key, $value] = explode('=', substr($arg, 2), 2);
        $options[$key] = $value;
    } else {
        $options[substr($arg, 2)] = true;
    }
}

$workers = (int) ($options['workers'] ?? \Swow\nproc());
$mode = (string) ($options['mode'] ?? 'inline');

try {
    switch ($command) {
        case 'archive':
            $source = $options['source'] ?? '';
            $blob = $options['blob'] ?? '';
            $compression = (string) ($options['compression'] ?? 'auto');
            if (!is_string($source) || !is_string($blob) || $source === '' || $blob === '') {
                usage();
            }
            Fastext::archive($source, $blob, $workers, $mode, $compression);
            echo "Archived {$source} to {$blob}\n";
            break;
        case 'extract':
            $blob = $options['blob'] ?? '';
            $dest = $options['dest'] ?? '';
            if (!is_string($blob) || !is_string($dest) || $blob === '' || $dest === '') {
                usage();
            }
            Fastext::extract($blob, $dest, $workers, $mode);
            echo "Extracted {$blob} to {$dest}\n";
            break;
        case 'verify':
            $source = $options['source'] ?? '';
            $dest = $options['dest'] ?? '';
            if (!is_string($source) || !is_string($dest) || $source === '' || $dest === '') {
                usage();
            }
            exit(Verify::compare($source, $dest) ? 0 : 1);
        default:
            usage();
    }
} catch (Throwable $e) {
    fwrite(STDERR, $e::class . ': ' . $e->getMessage() . "\n");
    exit(1);
}
