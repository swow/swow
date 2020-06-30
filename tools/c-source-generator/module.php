<?php

require __DIR__ . '/../../vendor/autoload.php';

use function Swow\Util\scanDir;

(function () {
    $replacement = [
        'file_name' => $argv[1] ?? 'foo',
        'module_name' => $argv[2] ?? 'foo',
        'type_name' => $argv[3] ?? 'foo',
        'cat_var_name' => $argv[4] ?? 'foo',
        'class_name' => $argv[5] ?? 'Foo'
    ];
    $replacement['php_var_name'] = "s{$replacement['swow_var_name']}";
    $replacement['class_name'] = addcslashes($replacement['class_name'], '\\');
    foreach ($replacement as $name => $value) {
        $replacement[strtoupper($name)] = strtoupper($value);
    }

    $result = '';
    $templatesFiles = scanDir(__DIR__, function (string $filename) {
        return pathinfo($filename, PATHINFO_EXTENSION) === 'template';
    });
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
