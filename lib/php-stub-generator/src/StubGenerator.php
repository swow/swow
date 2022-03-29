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

namespace Swow\StubUtils;

use InvalidArgumentException;
use ReflectionClass;
use ReflectionException;
use ReflectionExtension;
use ReflectionFunction;
use ReflectionFunctionAbstract;
use ReflectionIntersectionType;
use ReflectionMethod;
use ReflectionNamedType;
use ReflectionProperty;
use ReflectionUnionType;
use Reflector;
use RuntimeException;
use function array_map;
use function array_merge;
use function array_pop;
use function array_slice;
use function array_walk;
use function bin2hex;
use function class_exists;
use function count;
use function ctype_print;
use function dirname;
use function explode;
use function file_put_contents;
use function fwrite;
use function implode;
use function in_array;
use function interface_exists;
use function is_array;
use function is_bool;
use function is_dir;
use function is_float;
use function is_int;
use function is_null;
use function is_resource;
use function is_string;
use function ltrim;
use function method_exists;
use function sprintf;
use function str_contains;
use function str_repeat;
use function str_replace;
use function strlen;
use function substr;
use function trim;
use function var_export;

class StubGenerator
{
    protected const INDENT = '    ';

    protected ReflectionExtension $extension;

    /** @var array<string, array<string, int|float|bool|string|array<mixed>>> */
    protected array $constantMap;

    /** @var array<string> */
    protected array $headerComment = [];
    /** @var array<string> */
    protected array $noinspection = [];
    /** @var array<string, string> */
    protected array $userCommentMap = [];
    /* Disable some features because php-src gen_stub.php not support it */
    protected bool $genArginfoMode = false;
    /** @var callable[] */
    protected array $filters = [];

    public function __construct(string $extensionName)
    {
        $this->extension = new ReflectionExtension($extensionName);
    }

    public function getExtensionName(): string
    {
        return $this->extension->getName();
    }

    public function addHeaderCommentLine(string $line): static
    {
        $this->headerComment[] = $line;

        return $this;
    }

    /**
     * set no inspection options
     *
     * @param bool|array<string> $enable true to setup default no inspections(PhpUnused, PhpInconsistentReturnPointsInspection, PhpMissingParentConstructorInspection, PhpReturnDocTypeMismatchInspection), array of options to setup only these options
     */
    public function setNoinspection(bool|array $enable = true): static
    {
        if (is_bool($enable)) {
            $this->noinspection = $enable ? [
                'PhpUnused',
                'PhpInconsistentReturnPointsInspection',
                'PhpMissingParentConstructorInspection',
                'PhpReturnDocTypeMismatchInspection',
            ] : [];
        } else {
            $this->noinspection = $enable;
        }

        return $this;
    }

    /**
     * set comments
     *
     * @param array<string, string> $map
     */
    public function setUserCommentMap(array $map): static
    {
        $this->userCommentMap = $map;

        return $this;
    }

    public function setGenArginfoMode(bool $enable = true): static
    {
        $this->genArginfoMode = $enable;

        return $this;
    }

    /**
     * filter by filters
     *
     * @param callable(ReflectionFunction|ReflectionClass $functionOrClass): bool $filter
     */
    public function addFilter(callable $filter): static
    {
        $this->filters[] = $filter;

        return $this;
    }

    /**
     * @return array<string, array<string, ReflectionClass|ReflectionFunction>>
     */
    public function getExtensionFunctionAndClassReflectionsGroupByNamespace(): array
    {
        $reflections = [];
        foreach (array_merge($this->extension->getClasses(), $this->extension->getFunctions()) as $functionOrClass) {
            $reflections[$functionOrClass->getNamespaceName()][$functionOrClass->getShortName()] = $functionOrClass;
        }

        return $reflections;
    }

    /**
     * @param string|resource|null $output
     */
    public function generate(mixed $output = null): void
    {
        $declarations = [];

        foreach ($this->getExtensionFunctionAndClassReflectionsGroupByNamespace() as $namespaceName => $namespacedGroup) {
            if (!$this->genArginfoMode && $this->hasConstantInNamespace($namespaceName)) {
                $declarations[] = $this->generateConstantDeclarationOfNamespace($namespaceName);
            }
            foreach ($namespacedGroup as $functionOrClass) {
                if ($this->filters) {
                    foreach ($this->filters as $filter) {
                        if (!$filter($functionOrClass)) {
                            continue 2;
                        }
                    }
                }
                if ($functionOrClass instanceof ReflectionFunction) {
                    $declarations[] = $this->generateFunctionDeclaration($functionOrClass);
                } elseif ($functionOrClass instanceof ReflectionClass) {
                    $declarations[] = $this->generateClassDeclaration($functionOrClass);
                } else {
                    throw new InvalidArgumentException('Unknown type ' . $functionOrClass::class);
                }
            }
        }

        $declarations = implode("\n\n", $declarations);
        $headerComment = $this->headerComment;
        if ($this->noinspection) {
            $headerComment[] = '@noinspection ' . implode(', ', $this->noinspection);
        }
        if ($headerComment) {
            $headerComment = static::genComment($headerComment);
        }

        if ($declarations) {
            $content = implode("\n", [
                '<?php',
                '',
                ...($headerComment ? [$headerComment, ''] : ['']),
                $declarations,
                '',
            ]);
        } else {
            $content = '';
        }

        if (is_resource($output)) {
            fwrite($output, $content);
        } elseif (is_string($output) && is_dir(dirname($output))) {
            file_put_contents($output, $content);
        } else {
            echo $output;
        }
    }

    protected static function convertValueToString(mixed $value): string
    {
        return match (true) {
            is_int($value), is_float($value) => (string) ($value),
            is_null($value) => 'null',
            is_bool($value) => $value ? 'true' : 'false',
            is_string($value) => '\'' . (ctype_print($value) ? $value : bin2hex($value)) . '\'',
            is_array($value) => '[]',
            default => var_export($value, true),
        };
    }

    /**
     * get constant map like `[ '\Some\Namespace_' => [ 'SOME_CONST' => 1 ] ]`
     *
     * @return array<string, array<string, int|float|bool|string|array<mixed>>>
     */
    public function getConstantMap(): array
    {
        if (isset($this->constantMap)) {
            return $this->constantMap;
        }

        $constantMap = [];

        foreach ($this->extension->getConstants() as $constantName => $constantValue) {
            $namespaceName = explode('\\', $constantName);
            $constantName = array_pop($namespaceName);
            $namespaceName = implode('\\', $namespaceName);
            $constantMap[$namespaceName][$constantName] = $constantValue;
        }

        return $this->constantMap = $constantMap;
    }

    protected function hasConstantInNamespace(string $namespacedName): bool
    {
        return !empty($this->getConstantsOfNamespace($namespacedName));
    }

    /**
     * @return array<string, int|float|bool|string|array<mixed>>
     */
    protected function getConstantsOfNamespace(string $namespacedName): array
    {
        return $this->getConstantMap()[$namespacedName] ?? [];
    }

    protected function generateConstantDeclarationOfNamespace(string $namespaceName): string
    {
        $constantsOfNamespace = $this->getConstantsOfNamespace($namespaceName);

        if (empty($constantsOfNamespace)) {
            throw new RuntimeException("No constant in {$namespaceName}");
        }

        $declaration = [];
        $declaration[] = "namespace {$namespaceName}";
        $declaration[] = '{';
        foreach ($constantsOfNamespace as $constantName => $constantValue) {
            $declaration[] = "    const {$constantName} = {$this::convertValueToString($constantValue)};";
        }
        $declaration[] = '}';

        return implode("\n", $declaration);
    }

    protected function getDeclarationPrefix(ReflectionFunction|ReflectionMethod|ReflectionProperty|Reflector $reflector, bool $withSpace = false): string
    {
        $prefix = [];
        if (method_exists($reflector, 'isFinal') && $reflector->isFinal()) {
            $prefix[] = 'final';
        }
        if (method_exists($reflector, 'isAbstract') && $reflector->isAbstract()) {
            $prefix[] = 'abstract';
        }
        if (method_exists($reflector, 'isPublic') && $reflector->isPublic()) {
            $prefix[] = 'public';
        } elseif (method_exists($reflector, 'isProtected') && $reflector->isProtected()) {
            $prefix[] = 'protected';
        } elseif (method_exists($reflector, 'isPrivate') && $reflector->isPrivate()) {
            $prefix[] = 'private';
        }
        if (method_exists($reflector, 'isStatic') && $reflector->isStatic()) {
            $prefix[] = 'static';
        }
        if ($withSpace) {
            $prefix[] = '';
        }

        return implode(' ', $prefix);
    }

    /**
     * @param string|array<int, string> $comment
     */
    protected static function genComment(string|array $comment): string
    {
        if (is_array($comment)) {
            if ($comment) {
                if (count($comment) === 1) {
                    $comment = ['/** ' . trim($comment[0], ' *') . ' */'];
                } else {
                    $comment = ['/**', ...array_map(static fn (string $line): string => empty($line) ? ' *' : " * {$line}", $comment), ' */'];
                }
                $comment = implode("\n", $comment);
            } else {
                $comment = '';
            }
        }

        return $comment;
    }

    /**
     * @return string|array<string>
     */
    protected static function solveRawUserComment(string $userComment): string|array
    {
        // format user comment
        $userCommentLines = explode("\n", $userComment);
        if (count($userCommentLines) === 1) {
            $comment = trim($userComment);
        } else {
            $userCommentLines = array_slice($userCommentLines, 1, count($userCommentLines) - 2);
            $comment = array_map(static fn (string $line) => trim($line, '/* '), $userCommentLines);
        }

        return $comment;
    }

    /**
     * @param string|array<string> $comment
     */
    protected function formatFunction(ReflectionFunctionAbstract $function, string|array $comment, string $prefix, string $name, string $params, string $returnType, string $body): string
    {
        $comment = static::genComment($comment);
        return sprintf(
            '%s%s%s%sfunction %s(%s)%s%s {%s}',
            $comment,
            $comment ? "\n" : '',
            $prefix,
            $prefix ? ' ' : '',
            $name,
            $params,
            $returnType ? ': ' : '',
            $returnType,
            $body
        );
    }

    protected function generateFunctionDeclaration(ReflectionFunction|ReflectionMethod $function): string
    {
        $name = ltrim(str_replace($function->getNamespaceName(), '', $function->getName(), $isInNamespace), '\\');
        $scope = $function instanceof ReflectionMethod ? $function->getDeclaringClass()->getName() : '';

        $comment = [];
        $paramsDeclarations = [];
        $params = $function->getParameters();
        foreach ($params as $param) {
            $variadic = $param->isVariadic() ? '...' : '';
            /** @var ReflectionNamedType|ReflectionUnionType|ReflectionIntersectionType|null */
            $paramTypeReflection = $param->getType();
            if ($paramTypeReflection) {
                if ($paramTypeReflection instanceof ReflectionNamedType) {
                    $paramTypeNames = [$paramTypeReflection->getName()];
                } else {
                    $paramTypeNames = $paramTypeReflection->getTypes();
                }
            } else {
                $paramTypeNames = [];
            }
            $paramTypeNames = array_map(function (string $type) use ($scope) {
                if (!$this->genArginfoMode && $type === $scope) {
                    return 'self';
                }
                if (class_exists($type) || interface_exists($type)) {
                    return "\\{$type}";
                }
                return $type;
            }, $paramTypeNames);
            try {
                $paramDefaultValueIsNull = $param->getDefaultValue() === null;
            } catch (ReflectionException) {
                $paramDefaultValueIsNull = false;
            }
            switch (true) {
                case $paramTypeReflection instanceof ReflectionIntersectionType:
                    $paramTypeName = implode('&', $paramTypeNames);
                    break;
                default:
                    $paramTypeName = implode('|', $paramTypeNames);
                    break;
            }
            if (count($paramTypeNames) === 1 && $paramTypeName !== 'mixed' && ($param->allowsNull() || $paramDefaultValueIsNull)) {
                $paramTypeName = "?{$paramTypeName}";
            }
            try {
                $hasSpecialDefaultParamValue = false;
                $defaultParamValue = $param->getDefaultValue();
                $defaultParamConstantName = $param->getDefaultValueConstantName();
                $defaultParamValueString = $this::convertValueToString($defaultParamValue);
                if (is_string($defaultParamValue) && ($paramTypeName !== 'string' || preg_match('/[\W]/', $defaultParamValue) > 0)) {
                    $defaultParamValueTip = 'null';
                    $defaultParamValueTipOnDoc = trim($defaultParamValueString, '\'');
                    $hasSpecialDefaultParamValue = true;
                } else {
                    if (is_string($defaultParamConstantName) && $defaultParamConstantName !== '') {
                        $defaultParamValueTip = "\\{$defaultParamConstantName}";
                        if (!$this->genArginfoMode && str_contains($defaultParamValueTip, '::')) {
                            $parts = explode('::', $defaultParamValueTip);
                            $class = ltrim($parts[0], '\\');
                            if ($class === $scope) {
                                $defaultParamValueTip = "self::{$parts[1]}";
                            }
                        }
                    } elseif ($defaultParamValueString !== '') {
                        $defaultParamValueTip = $defaultParamValueString;
                    } else {
                        $defaultParamValueTip = '';
                    }
                    $defaultParamValueTipOnDoc = $defaultParamValueTip;
                }
            } catch (ReflectionException) {
                $defaultParamValueTip = $defaultParamValueTipOnDoc = '';
            }
            if ($function instanceof ReflectionFunction) {
                $fullName = $function->getName();
            } else /* if ($function instanceof ReflectionMethod) */ {
                $operator = $function->isStatic() ? '::' : '->';
                $fullName = "{$function->getDeclaringClass()->getName()}{$operator}{$function->getShortName()}";
            }
            if (!$this->genArginfoMode) {
                $userComment = $this->userCommentMap[$fullName] ?? '';
                if ($userComment) {
                    $comment = static::solveRawUserComment($userComment);
                } elseif ($hasSpecialDefaultParamValue) {
                    $comment[] = sprintf(
                        '@param %s%s%s$%s%s%s',
                        $paramTypeName,
                        $paramTypeName ? ' ' : '',
                        $variadic,
                        $param->getName(),
                        !$variadic ? ($param->isOptional() ? ' [optional]' : ' [required]') : '',
                        $defaultParamValueTipOnDoc !== '' ? " = {$defaultParamValueTipOnDoc}" : ''
                    );
                }
            }
            $paramsDeclarations[] = sprintf(
                '%s%s%s%s$%s%s',
                $paramTypeName,
                $paramTypeName ? ' ' : '',
                $variadic,
                $param->isPassedByReference() ? '&' : '',
                $param->getName(),
                $defaultParamValueTip !== '' ? " = {$defaultParamValueTip}" : ''
            );
        }
        $paramsDeclaration = implode(', ', $paramsDeclarations);

        if ($function->hasReturnType()) {
            /** @var ReflectionNamedType|ReflectionUnionType|ReflectionIntersectionType|null */
            $returnType = $function->getReturnType();
            if ($returnType instanceof ReflectionNamedType) {
                $returnTypeNames = [$returnType->getName()];
            } else {
                $returnTypeNames = $returnType->getTypes();
            }
            array_walk($returnTypeNames, function (string &$returnTypeName) use ($function): void {
                if (class_exists($returnTypeName) || interface_exists($returnTypeName)) {
                    if (!$this->genArginfoMode && $function instanceof ReflectionMethod && $returnTypeName === $function->getDeclaringClass()->getName()) {
                        $returnTypeName = 'self';
                    } else {
                        if ($function instanceof ReflectionMethod) {
                            $namespace = $function->getDeclaringClass()->getNamespaceName();
                        } else {
                            $namespace = $function->getNamespaceName();
                        }
                        if ($namespace) {
                            $returnTypeShortName = ltrim(str_replace($namespace, '', $returnTypeName, $count), '\\');
                            if ($count === 0 || $returnTypeShortName === '') {
                                goto _full_namespace;
                            }
                            $returnTypeName = $returnTypeShortName;
                        } else {
                            _full_namespace:
                            $returnTypeName = '\\' . $returnTypeName;
                        }
                    }
                }
            });
            switch (true) {
                case $returnType instanceof ReflectionIntersectionType:
                    $returnTypeName = implode('&', $returnTypeNames);
                    break;
                default:
                    $returnTypeName = implode('|', $returnTypeNames);
                    break;
            }
            if ($returnTypeName !== 'mixed' && !in_array('null', $returnTypeNames, true) && $returnType->allowsNull()) {
                $returnTypeName = "?{$returnTypeName}";
            }
        } else {
            $returnTypeName = '';
        }
        $body = ' ';

        $declaration = $this->formatFunction(
            $function,
            $comment,
            $this->getDeclarationPrefix($function),
            $name,
            $paramsDeclaration,
            $returnTypeName,
            $body
        );

        if (!method_exists($function, 'getDeclaringClass')) {
            $namespace = $isInNamespace ? "namespace {$function->getNamespaceName()}" : 'namespace';
            $declaration = $this::indent($declaration, 1);
            $declaration = "{$namespace}\n{\n{$declaration}\n}";
        }

        return $declaration;
    }

    protected function generateClassDeclaration(ReflectionClass $class): string
    {
        $prefix = $this->getDeclarationPrefix($class, true);
        $type = $class->isInterface() ? 'interface' : ($class->isTrait() ? 'trait' : 'class');
        $namespaceName = $class->getNamespaceName();
        $shortName = $class->getShortName();
        $parent = $class->getParentClass();
        $parentName = $parent ? $parent->getName() : '';
        $extends = $parentName ? " extends \\{$parentName}" : '';
        $interfaceNames = $class->getInterfaceNames();
        if ($parent) {
            foreach ($interfaceNames as $index => $interfaceName) {
                if ($parent->implementsInterface($interfaceName)) {
                    unset($interfaceNames[$index]);
                }
            }
        }
        if (count($interfaceNames) > 0) {
            $implements = ' implements ' . '\\' . implode(', \\', $interfaceNames);
        } else {
            $implements = '';
        }

        $constantDeclarations = [];
        if (!$this->genArginfoMode) {
            foreach ($class->getConstants() as $constantName => $constantValue) {
                $userComment = $this->userCommentMap["{$class->getName()}::{$constantName}"] ?? null;
                $comment = $userComment ? static::genComment(static::solveRawUserComment($userComment)) : '';
                $constantDeclarations[] = sprintf(
                    '%s%spublic const %s = %s',
                    $comment,
                    $comment ? "\n" : '',
                    $constantName,
                    $this::convertValueToString($constantValue)
                );
            }
        }

        $propertyDeclarations = [];
        foreach ($class->getProperties() as $property) {
            if ($property->getDeclaringClass()->getName() !== $class->getName()) {
                continue;
            }
            $propertyParts = [];
            $propertyParts[] = $this->getDeclarationPrefix($property);
            if ($propertyType = $property->getType()) {
                switch (true) {
                    case $propertyType instanceof ReflectionNamedType:
                        $propertyParts[] = $propertyType->getName();
                        break;
                    case $propertyType instanceof ReflectionUnionType:
                        $propertyParts[] = implode('|', $propertyType->getTypes());
                        break;
                    case $propertyType instanceof ReflectionIntersectionType:
                        $propertyParts[] = implode('&', $propertyType->getTypes());
                        break;
                }
            }
            $propertyParts[] = "\${$property->getName()}";
            $propertyDeclarations[] = implode(' ', $propertyParts);
        }

        $methodDeclarations = [];
        foreach ($class->getMethods() as $method) {
            if ($method->getDeclaringClass()->getName() !== $class->getName()) {
                continue;
            }
            $methodDeclarations[] = $this->generateFunctionDeclaration($method);
        }

        /* implode */
        $information = [
            ['declarations' => $constantDeclarations, 'isAssignment' => true],
            ['declarations' => $propertyDeclarations, 'isAssignment' => true],
            ['declarations' => $methodDeclarations],
        ];
        $body = [];
        foreach ($information as $part) {
            if (count($part['declarations']) > 0) {
                $glue = ($part['isAssignment'] ?? false) ? ';' : "\n";
                $body[] = implode("{$glue}\n", $part['declarations']) . $glue;
            }
        }
        $body = ltrim($this::indent(trim(implode("\n\n", $body), "\n"), 2));

        $namespaceNameDeclaration = $namespaceName ? "namespace {$namespaceName}" : 'namespace';

        if (!empty($body)) {
            $body =
                "\n" .
                "    {\n" .
                "        {$body}\n" .
                "    }\n";
        } else {
            $body =
                " { }\n";
        }

        $userComment = $this->userCommentMap[$class->getName()] ?? null;
        if (!$this->genArginfoMode && $userComment) {
            $commentIndent = static::INDENT;
            $comment = static::genComment(static::solveRawUserComment($userComment));
            $commentLF = "\n";
        } else {
            $commentIndent = $comment = $commentLF = '';
        }

        return
            "{$namespaceNameDeclaration}\n" .
            "{\n" .
            "{$commentIndent}{$comment}{$commentLF}" .
            "    {$prefix}{$type} {$shortName}{$extends}{$implements}{$body}" .
            '}';
    }

    /**
     * @param non-empty-string|null $eol
     */
    protected static function indent(string $content, int $level, ?string $eol = null): string
    {
        $eol ??= "\n";
        $spaces = str_repeat(static::INDENT, $level);
        $lines = explode($eol, $content);
        $content = '';
        foreach ($lines as $line) {
            if ($line === '') {
                $content .= $eol;
            } else {
                $content .= $spaces . $line . $eol;
            }
        }
        if ($content !== '') {
            $content = substr($content, 0, strlen($content) - strlen($eol));
        }

        return $content;
    }
}
