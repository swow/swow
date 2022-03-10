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

foreach ([0, 2] as $level) {
    foreach ([dirname(__DIR__, 1 + $level) . '/vendor/autoload.php', dirname(__DIR__, 3 + $level) . '/autoload.php'] as $file) {
        if (file_exists($file)) {
            require $file;
            break;
        }
    }
}

use Swow\StubUtils\StubGenerator;
use function Swow\Util\processExecute;

$genStub = static function (): void {
    global $argv;
    /**
     * @var array<string, string> $options
     */
    $options = getopt('h', ['help', 'noinspection', 'stub-file::', 'gen-arginfo-mode', 'filter-mode', 'function-filter::', 'class-filter::'], $restIndex);
    $argv = array_slice($argv, $restIndex);
    $n = $argv[0] ?? '';
    $error = empty($n);
    if (isset($options['h']) || isset($options['help']) || $error) {
        $basename = basename(__FILE__);
        echo <<<TEXT
Usage: php {$basename} \\
         [-h|--help] [--noinspection] [--stub-file=/path/to/ext.stub.php] [--gen-arginfo-mode] \\
         [--filter-mode] [--function-filter=functionA|functionB] [--class-filter=classA|classB] \\
         <extension-name> [output-target]

TEXT;
        exit($error ? 1 : 0);
    }
    if (!empty($options['stub-file']) && !file_exists($options['stub-file'])) {
        echo "Stub file '{$options['stub-file']}' not exist";
        exit(1);
    }
    $g = new StubGenerator($n);
    if (isset($options['noinspection'])) {
        $g->setNoinspection();
    }
    if (isset($options['stub-file'])) {
        $constantMap = $g->getConstantMap();
        $declarationMap = $g->getExtensionFunctionAndClassReflectionsGroupByNamespace();
        $declarationMap = array_map(static fn (array $reflections) => array_keys($reflections), $declarationMap);
        $data = serialize([
            'constants' => $constantMap,
            'declarations' => $declarationMap,
        ]);
        $userCommentMap = processExecute([
            PHP_BINARY,
            '-n',
            __FILE__,
            'get_stub_comments',
            $options['stub-file'],
            $data,
        ]);
        $userCommentMap = unserialize($userCommentMap);
        $g->setUserCommentMap($userCommentMap);
    }
    if (isset($options['gen-arginfo-mode'])) {
        $g->setGenArginfoMode();
    }
    if (isset($options['filter-mode']) || isset($options['function-filter']) || isset($options['class-filter'])) {
        $getFilterMap = static function (string $filterString): array {
            $filterList = array_filter(array_map('trim', explode('|', $filterString)));
            $filterMap = [];
            foreach ($filterList as $item) {
                $filterMap[$item] = true;
            }
            return $filterMap;
        };
        $filterMode = isset($options['filter-mode']);
        $functionFilterMap = $getFilterMap($options['function-filter'] ?? '');
        $classFilterMap = $getFilterMap($options['class-filter'] ?? '');
        $g->addFilter(static function (ReflectionFunction|ReflectionClass $reflection) use ($filterMode, $functionFilterMap, $classFilterMap) {
            $name = str_replace('\\', '_', $reflection->getName());
            if ($functionFilterMap && $reflection instanceof ReflectionFunction) {
                return isset($functionFilterMap[$name]);
            }
            if ($classFilterMap && $reflection instanceof ReflectionClass) {
                return isset($classFilterMap[$name]);
            }
            return !$filterMode;
        });
    }

    $g->generate($argv[1] ?? STDOUT);
};

$getStubComments = function () use ($argv): void {
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
            $functionOrClassName = $namespace ? "{$namespace}\\{$functionOrClass}" : $functionOrClass;
            if (class_exists($functionOrClassName)) {
                $classReflection = new ReflectionClass($functionOrClassName);
                $comment = $classReflection->getDocComment();
                if ($comment) {
                    $commentMap[$functionOrClassName] = $comment;
                }
                $classConstantReflections = $classReflection->getReflectionConstants();
                foreach ($classConstantReflections as $classConstantReflection) {
                    $comment = $classConstantReflection->getDocComment();
                    if ($comment) {
                        $classConstantName = "{$functionOrClassName}::{$classConstantReflection->getName()}";
                        $commentMap[$classConstantName] = $comment;
                    }
                }
                $methodsReflections = $classReflection->getMethods();
                foreach ($methodsReflections as $methodsReflection) {
                    $comment = $methodsReflection->getDocComment();
                    if ($comment) {
                        $operator = $methodsReflection->isStatic() ? '::' : '->';
                        $methodName = "{$functionOrClassName}{$operator}{$methodsReflection->getShortName()}";
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
