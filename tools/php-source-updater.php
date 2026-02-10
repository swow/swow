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

(static function () use ($argv): void {
    if (count($argv) !== 2) {
        echo "Usage: php tools/php-source-updater.php <php source dir with git>\n";
        exit(1);
    }
    $workdir = realpath(__DIR__ . '/../');
    $sourceDir = $workdir . '/ext/src';
    $phpSourceDir = realpath($argv[1]);

    foreach (glob($sourceDir . '/*.{c,h}', GLOB_BRACE) as $file) {
        $relFile = str_replace($workdir . '/', '', $file);
        $content = file_get_contents($file);
        preg_match_all('|// from ([^ ]*) @ ([a-f0-9]{40})|', $content, $matches);
        if ($matches === false) {
            // ignore it
            continue;
        }
        foreach ($matches[0] as $i => $_) {
            $fromFile = $matches[1][$i];
            $fromCommit = $matches[2][$i];

            printf("Updating %s from %s @ %s\n", $relFile, $fromFile, $fromCommit);

            // get file head commit
            $headCommitCmd = [
                'git',
                '-C',
                $phpSourceDir,
                'log',
                '--format=%H',
                '-1',
                "{$fromFile}",
            ];
            $proc = proc_open($headCommitCmd, [
                0 => STDIN,
                1 => ['pipe', 'w'],
                2 => STDERR,
            ], $pipes);
            $headCommit = trim(stream_get_contents($pipes[1]));
            fclose($pipes[1]);
            proc_close($proc);

            if ($headCommit === $fromCommit) {
                printf("No diff since %s\n", $fromCommit);
                continue;
            }

            // get diff
            $diffCmd = [
                'git',
                '-C',
                $phpSourceDir,
                'diff',
                "{$fromCommit}..HEAD",
                '--',
                "{$fromFile}",
            ];
            $proc = proc_open($diffCmd, [
                0 => STDIN,
                1 => ['pipe', 'w'],
                2 => STDERR,
            ], $pipes);
            $diff = stream_get_contents($pipes[1]);
            fclose($pipes[1]);
            proc_close($proc);
            if ($diff === '') {
                printf("No diff found for %s\n", $relFile);
                continue;
            }
            $diffLines = explode("\n", $diff);
            $diffLines[0] = '';
            $diffLines[1] = '';
            $diffLines[2] = "--- a/{$relFile}";
            $diffLines[3] = "+++ b/{$relFile}";
            // \t -> 4 spaces
            foreach ($diffLines as $i => $line) {
                $diffLines[$i] = str_replace("\t", '    ', $line);
            }
            $diff = implode("\n", $diffLines);

            // show diff
            printf("// from %s @ %s\n", $fromFile, $headCommit);
            printf('%s', $diff);
            printf("patch ? (y/n)\n");
            $patch = trim(fgets(STDIN));
            if ($patch !== 'y') {
                continue;
            }

            // update comment
            $content = preg_replace('|// from ([^ ]*) @ ([a-f0-9]{40})|', "// from {$fromFile} @ {$headCommit}", $content);
            file_put_contents($file, $content);

            // try patch
            $patchCmd = [
                'patch',
                '-p1',
            ];
            $proc = proc_open($patchCmd, [
                0 => ['pipe', 'r'],
                1 => STDOUT,
                2 => STDERR,
            ], $pipes);
            fwrite($pipes[0], $diff);
            fclose($pipes[0]);
            proc_close($proc);
        }
    }
})();
