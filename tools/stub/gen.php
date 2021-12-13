<?php

require __DIR__ . '/../autoload.php';

use Swow\Coroutine;
use Swow\Extension\StubGenerator;
use Swow\Util\FileSystem;

(function () {
    global $argv;
    if (empty($argv[1])) {
        exit('Usage: php ' . basename(__FILE__) . ' <extension_name> [output_path]');
    }
    $n = $argv[1];
    $g = new StubGenerator($n);
    if (strcasecmp($g->getExtensionName(), 'Swow') === 0) {
        $returnThisMap = [];
        $sourceFiles = FileSystem::scanDir(__DIR__ . '/../../ext/src', function (string $filename) {
            return pathinfo($filename, PATHINFO_EXTENSION) === 'c';
        });
        foreach ($sourceFiles as $SourceFile) {
            $source = file_get_contents($SourceFile);
            $getFullName = function (string $argInfoName): string {
                $argInfoNameParts = explode('_', $argInfoName);
                assert(count($argInfoNameParts) > 1);
                $methodName = array_pop($argInfoNameParts);
                $className = implode('\\', $argInfoNameParts);
                return "{$className}::{$methodName}";
            };
            preg_match_all(
                '/ZEND_BEGIN_ARG_WITH_RETURN_THIS(?<allow_null>_OR_NULL)?_INFO_EX\(arginfo_class_(?<name>\w+), \d+\)/',
                $source, $matches, PREG_SET_ORDER
            );
            foreach ($matches as $match) {
                $returnThisMap[$getFullName($match['name'])] = [
                    'allow_null' => $match['allow_null'] !== ''
                ];
            }
            preg_match_all(
                '/#define arginfo_class_(?<A>\w+) arginfo_class_(?<B>\w+)/',
                $source, $matches, PREG_SET_ORDER
            );
            foreach ($matches as $match) {
                $bName = $getFullName($match['B']);
                if (isset($returnThisMap[$bName])) {
                    $aName = $getFullName($match['A']);
                    $returnThisMap[$aName] = $returnThisMap[$bName];
                }
            }
            preg_match_all(
                '/PHP_ME\((?<class_name>\w+), (?<method_name>\w+), +arginfo_class_(?<B>\w+),/',
                $source, $matches, PREG_SET_ORDER
            );
            foreach ($matches as $match) {
                $bName = $getFullName($match['B']);
                if (isset($returnThisMap[$bName])) {
                    $aName = "{$match['class_name']}_{$match['method_name']}";
                    $aName = $getFullName($aName);
                    $returnThisMap[$aName] = $returnThisMap[$bName];
                }
            }
        }
        $g->setFunctionFormatHandler(function (ReflectionFunctionAbstract $function, string &$comment, string $prefix, string $name, string $params, string $returnType, string $body) use (
            $returnThisMap
        ) {
            if ($function instanceof ReflectionMethod) {
                $className = $function->getDeclaringClass()->getName();
                $fullName = "{$className}::{$name}";
                $returnThisInfo = $returnThisMap[$fullName] ?? null;
                if ($returnThisInfo !== null) {
                    $nullable = $returnThisInfo['allow_null'] ? '?' : '';
                    $comment = str_replace(
                        '@return mixed',
                        "@return {$nullable}\$this",
                        $comment
                    );
                } elseif ($fullName === Coroutine::class . '::getAll') {
                    $comment = str_replace(
                        '@return array',
                        '@return ' . '\\' . Coroutine::class . '[]',
                        $comment
                    );
                }
            }
        });
        $g->withNoinspection();
    }
    $g->generate($argv[2] ?? STDOUT);
})();
