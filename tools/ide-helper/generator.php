<?php

require __DIR__ . '/../autoload.php';

use Swow\Coroutine;
use Swow\Util\IDE\ExtensionGenerator;

(function () {
    global $argv;
    if (empty($argv[1])) {
        exit('Usage: php ' . basename(__FILE__) . ' <extension_name> [output_path]');
    }
    $n = $argv[1];
    $g = new ExtensionGenerator($n);
    if (strcasecmp($g->getExtensionName(), 'Swow') === 0) {
        $g->setFunctionFormatHandler(function (ReflectionFunctionAbstract $function, string &$comment, string $prefix, string $name, string $params, string $returnType, string $body) {
            if ($function instanceof ReflectionMethod) {
                if ($function->getDeclaringClass()->getName() === Coroutine::class) {
                    if ($name === 'getAll') {
                        $comment = str_replace(
                            '@return array',
                            '@return ' . '\\' . Coroutine::class . '[]',
                            $comment
                        );
                    }
                }
            }
        });
    }
    $g->generate($argv[2] ?? STDOUT);
})();
