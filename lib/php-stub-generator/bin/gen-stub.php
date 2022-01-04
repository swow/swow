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

foreach (
    [
        dirname(__DIR__) . '/vendor/autoload.php',
        dirname(__DIR__, 3) . '/autoload.php',
        dirname(__DIR__, 3) . '/vendor/autoload.php',
        dirname(__DIR__, 5) . '/autoload.php',
    ] as $file
) {
    if (file_exists($file)) {
        require $file;
        break;
    }
}

use Swow\StubGenerator;
use function Swow\Util\processExecute;

$genStub = function () {
    global $argv;
    $options = getopt('h', ['help', 'noinspection', 'stub-file::', 'gen-arginfo-mode', 'function-filter::', 'class-filter::'], $restIndex);
    $argv = array_slice($argv, $restIndex);
    if (isset($options['h']) || isset($options['help']) || empty($argv[0])) {
        $basename = basename(__FILE__);
        exit(
        <<<TEXT
Usage: php {$basename} \\
         [-h|--help] [--noinspection] [--stub-file=/path/to/ext.stub.php] [--gen-arginfo-mode] \\
         [--function-filter=functionA|functionB] [--class-filter=classA|classB] \\
         <extension-name> [output-target]

TEXT
        );
    }
    if (!empty($options['stub-file']) && !file_exists($options['stub-file'])) {
        exit("Stub file '{$options['stub-file']}' not exist");
    }
    $n = $argv[0];
    $g = new StubGenerator($n);
    if (isset($options['noinspection'])) {
        $g->setNoinspection();
    }
    if (isset($options['stub-file'])) {
        $constantMap = $g->getConstantMap();
        $declarationMap = $g->getExtensionFunctionAndClassReflectionsGroupByNamespace();
        $declarationMap = array_map(fn (array $reflections) => array_keys($reflections), $declarationMap);
        $data = serialize([
            'constants' => $constantMap,
            'declarations' => $declarationMap,
        ]);
        $commentMap = processExecute([
            PHP_BINARY,
            '-n',
            __FILE__,
            'get_stub_comments',
            $options['stub-file'],
            $data,
        ]);
        $commentMap = unserialize($commentMap);
        $g->setCommentMap($commentMap);
    }
    if (isset($options['gen-arginfo-mode'])) {
        $g->setGenArginfoMode();
    }
    if (isset($options['function-filter']) || isset($options['class-filter'])) {
        $getFilterMap = function (string $filterString) {
            $filterList = array_map('trim', explode('|', $filterString));
            $filterMap = [];
            foreach ($filterList as $item) {
                $filterMap[$item] = true;
            }
            return $filterMap;
        };
        $functionFilterMap = $getFilterMap($options['function-filter'] ?? '');
        $classFilterMap = $getFilterMap($options['class-filter'] ?? '');
        $g->addFilter(function (ReflectionFunction|ReflectionClass $reflection) use ($functionFilterMap, $classFilterMap) {
            $name = str_replace('\\', '_', $reflection->getName());
            if ($functionFilterMap && $reflection instanceof ReflectionFunction) {
                return isset($functionFilterMap[$name]);
            }
            if ($classFilterMap /* && $reflection instanceof ReflectionMethod */) {
                return isset($classFilterMap[$name]);
            }
            return true;
        });
    }

    $g->generate($argv[1] ?? STDOUT);
};

$getStubComments = function () {
    global $argv;
    $stubFile = $argv[2];
    require $stubFile;
    $data = unserialize($argv[3]);
    // $constantMap = $data['constants'];
    $declarationMap = $data['declarations'];
    $commentMap = [];
    foreach ($declarationMap as $namespace => $group) {
        // TODO: we can not get comment of constant for now (use regex?)
        // if (isset($constantMap[$namespace])) { }
        foreach ($group as $functionOrClass) {
            $functionOrClassName = "{$namespace}\\{$functionOrClass}";
            if (class_exists($functionOrClassName)) {
                $classReflection = new ReflectionClass($functionOrClassName);
                $methodsReflections = $classReflection->getMethods();
                foreach ($methodsReflections as $methodsReflection) {
                    $comment = $methodsReflection->getDocComment();
                    $methodName = "{$functionOrClassName}->{$methodsReflection->getShortName()}";
                    if ($comment) {
                        $commentMap[$methodName] = $comment;
                    }
                }
            } elseif (function_exists($functionOrClassName)) {
                $functionReflection = new ReflectionFunction($functionOrClassName);
                $comment = $functionReflection->getDocComment();
                if ($comment) {
                    $commentMap[$functionOrClassName] = $comment;
                }
            }
        }
    }
    echo serialize($commentMap);
};

if (($argv[1] ?? '') === 'get_stub_comments') {
    $getStubComments();
} else {
    $genStub();
}
