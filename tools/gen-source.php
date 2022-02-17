#!/usr/bin/env php
<?php
/**
 * This file is part of Swow
 *
 * @link     https://github.com/swow/swow
 * @contact  twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

require __DIR__ . '/autoload.php';

use Swow\Util\FileSystem;

(static function (): void {
    global $argv;
    $replacement = [
        'file_name' => $argv[1] ?? 'foo',
        'module_name' => $argv[2] ?? 'foo',
        'type_name' => $argv[3] ?? 'foo',
        'cat_var_name' => $argv[4] ?? 'foo',
        'class_name' => $argv[5] ?? 'Foo',
    ];
    $replacement['php_var_name'] = "s{$replacement['cat_var_name']}";
    $replacement['class_name'] = addcslashes($replacement['class_name'], '\\');
    foreach ($replacement as $name => $value) {
        $replacement[strtoupper($name)] = strtoupper($value);
    }

    $result = '';
    $templatesFiles = FileSystem::scanDir(__DIR__ . '/templates');
    foreach ($templatesFiles as $filename) {
        $content = file_get_contents($filename);
        foreach ($replacement as $name => $value) {
            $content = str_replace("{{{$name}}}", $value, $content);
        }
        $result .= sprintf(
            "[%s]\n%s\n%s\n\n",
            basename($filename),
            str_repeat('-', 64),
            $content
        );
    }
    echo $result;
})();
