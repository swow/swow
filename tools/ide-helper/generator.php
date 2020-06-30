<?php

require __DIR__ . '/../../vendor/autoload.php';

use Swow\Util\IDE\ExtensionGenerator;

(function () {
    global $argv;
    if (empty($argv[1])) {
        exit('Usage: php ' . basename(__FILE__) . ' <extension_name> [output_path]');
    }
    $n = $argv[1];
    $g = new ExtensionGenerator($n);
    $g->generate($argv[2] ?? STDOUT);
})();
