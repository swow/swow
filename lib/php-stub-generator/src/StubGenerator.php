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

namespace Swow;

use InvalidArgumentException;
use ReflectionClass;
use ReflectionExtension;
use ReflectionFunction;
use ReflectionFunctionAbstract;
use ReflectionMethod;
use ReflectionProperty;
use Reflector;
use RuntimeException;
use Throwable;
use function array_merge;
use function array_pop;
use function bin2hex;
use function class_exists;
use function count;
use function ctype_print;
use function dirname;
use function explode;
use function file_put_contents;
use function fwrite;
use function get_class;
use function implode;
use function interface_exists;
use function is_array;
use function is_bool;
use function is_dir;
use function is_float;
use function is_int;
use function is_null;
use function is_numeric;
use function is_resource;
use function is_string;
use function ltrim;
use function method_exists;
use function rtrim;
use function sprintf;
use function str_repeat;
use function str_replace;
use function strlen;
use function strpos;
use function substr;
use function trim;
use function var_export;

class StubGenerator
{
    protected const INDENT = '    ';

    protected const MODIFIERS_MAP = [
        ReflectionProperty::IS_PUBLIC => 'public',
        ReflectionProperty::IS_PUBLIC | ReflectionProperty::IS_STATIC => 'public static',
        ReflectionProperty::IS_PROTECTED => 'protected',
        ReflectionProperty::IS_PROTECTED | ReflectionProperty::IS_STATIC => 'protected static',
        ReflectionProperty::IS_PRIVATE => 'private',
        ReflectionProperty::IS_PRIVATE | ReflectionProperty::IS_STATIC => 'private static',
    ];

    /** @var string */
    protected $extension;

    /** @var array */
    protected $constantMap;

    /** @var callable */
    protected $functionFormatHandler;

    /** @var bool */
    protected $noinspection = false;

    public function __construct(string $extensionName)
    {
        $this->extension = new ReflectionExtension($extensionName);
    }

    public function getExtensionName(): string
    {
        return $this->extension->getName();
    }

    /**
     * @return $this
     */
    public function setFunctionFormatHandler(callable $formatter)
    {
        $this->functionFormatHandler = $formatter;

        return $this;
    }

    /** @return $this */
    public function withNoinspection()
    {
        $this->noinspection = true;

        return $this;
    }

    /** @return $this */
    public function withOutNoinspection()
    {
        $this->noinspection = false;

        return $this;
    }

    public function generate($output = null): void
    {
        $declarations = [];

        $namespacedGroups = [];
        foreach (array_merge($this->extension->getClasses(), $this->extension->getFunctions()) as $functionOrClass) {
            $namespacedGroups[$functionOrClass->getNamespaceName()][] = $functionOrClass;
        }

        foreach ($namespacedGroups as $namespaceName => $namespacedGroup) {
            if ($this->hasConstantsInNamespace($namespaceName)) {
                $declarations[] = $this->generateConstantDeclarationOfNamespace($namespaceName);
            }
            foreach ($namespacedGroup as $functionOrClass) {
                if ($functionOrClass instanceof ReflectionFunction) {
                    $declarations[] = $this->generateFunctionDeclaration($functionOrClass);
                } elseif ($functionOrClass instanceof ReflectionClass) {
                    $declarations[] = $this->generateClassDeclaration($functionOrClass);
                } else {
                    throw new InvalidArgumentException('Unknown type ' . get_class($functionOrClass));
                }
            }
        }

        $declarations = implode("\n\n", $declarations);

        $content = implode("\n", [
            '<?php',
            $this->noinspection ? '/** @noinspection PhpUnused, PhpInconsistentReturnPointsInspection, PhpMissingParentConstructorInspection, PhpReturnDocTypeMismatchInspection */' : '',
            '',
            $declarations,
        ]);

        if (is_resource($output)) {
            fwrite($output, $content);
        } elseif (is_string($output) && is_dir(dirname($output))) {
            file_put_contents($output, $content);
        } else {
            echo $output;
        }
    }

    protected static function convertValueToString($value): string
    {
        switch (1) {
            case is_int($value):
            case is_float($value):
                return (string) ($value);
            case is_null($value):
                return 'null';
            case is_bool($value):
                return $value ? 'true' : 'false';
            case is_string($value):
                return '\'' . (ctype_print($value) ? $value : bin2hex($value)) . '\'';
            case is_array($value):
                return '[]';
            default:
                return var_export($value, true);
        }
    }

    protected function getConstantMap(): array
    {
        if ($this->constantMap !== null) {
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

    protected function hasConstantsInNamespace(string $namespacedName): bool
    {
        return !empty($this->getConstantsOfNamespace($namespacedName));
    }

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

        $declaration = "namespace {$namespaceName}\n{\n";
        foreach ($constantsOfNamespace as $constantName => $constantValue) {
            $declaration .= "    const {$constantName} = {$this::convertValueToString($constantValue)};\n";
        }
        $declaration .= '}';

        return $declaration;
    }

    /** @param ReflectionFunction|ReflectionMethod|ReflectionProperty|Reflector $reflector */
    protected function getDeclarationPrefix(Reflector $reflector, bool $withSpace = false): string
    {
        $prefix = '';
        if (method_exists($reflector, 'isFinal') && $reflector->isFinal()) {
            $prefix .= 'final ';
        }
        if (method_exists($reflector, 'isAbstract') && $reflector->isAbstract()) {
            $prefix .= 'abstract ';
        }
        if (method_exists($reflector, 'getModifiers')) {
            $modifier = $this::MODIFIERS_MAP[$reflector->getModifiers()] ?? '';
            if ($modifier) {
                $prefix .= $modifier . ' ';
            }
        }
        if (!$withSpace) {
            $prefix = rtrim($prefix, ' ');
        }

        return $prefix;
    }

    protected function formatFunction(ReflectionFunctionAbstract $function, string $comment, string $prefix, string $name, string $params, string $returnType, string $body): string
    {
        $handler = $this->functionFormatHandler;
        if ($handler) {
            $result = $handler($function, $comment, $prefix, $name, $params, $returnType, $body);
            if ($result) {
                return $result;
            }
        }

        return sprintf('%s%s function %s(%s)%s {%s}', $comment, $prefix, $name, $params, $returnType, $body);
    }

    /**
     * @param ReflectionFunction|ReflectionMethod $function
     * @param string $prefix
     */
    protected function generateFunctionDeclaration(ReflectionFunctionAbstract $function): string
    {
        $name = ltrim(str_replace($function->getNamespaceName(), '', $function->getName(), $isInNamespace), '\\');

        $comment = '';
        $paramsDeclarations = [];
        $params = $function->getParameters();
        foreach ($params as $param) {
            $variadic = $param->isVariadic() ? '...' : '';
            $paramType = ltrim((string) $param->getType(), '?') ?: 'mixed';
            if (class_exists($paramType) || interface_exists($paramType)) {
                $paramType = '\\' . $paramType;
            }
            $paramIsUnion = strpos($paramType, '|') !== false;
            $nullTypeHint = $paramType !== 'mixed' && !$paramIsUnion && $param->allowsNull();
            try {
                $defaultParamValue = $param->getDefaultValue();
                $defaultParamConstantName = $param->getDefaultValueConstantName();
                $defaultParamValueString = $this::convertValueToString($defaultParamValue);
                if (is_string($defaultParamValue) && ($paramType !== 'string' || preg_match('/[\W]/', $defaultParamValue) > 0)) {
                    $defaultParamValueTip = 'null';
                    $defaultParamValueTipOnDoc = trim($defaultParamValueString, '\'');
                } else {
                    if (is_string($defaultParamConstantName) && $defaultParamConstantName !== '') {
                        $defaultParamValueTip = "\\{$defaultParamConstantName}";
                    } elseif ($defaultParamValueString !== '') {
                        $defaultParamValueTip = (string) $defaultParamValueString;
                    } else {
                        $defaultParamValueTip = $defaultParamValueTipOnDoc = '';
                    }
                    $defaultParamValueTipOnDoc = $defaultParamValueTip;
                }
            } catch (Throwable $throwable) {
                $defaultParamValueTip = $defaultParamValueTipOnDoc = '';
            }
            $comment .= sprintf(
                " * @param %s%s %s\$%s%s%s\n",
                $nullTypeHint ? 'null|' : '',
                $paramType,
                $variadic,
                $param->getName(),
                !$variadic ? ($param->isOptional() ? ' [optional]' : ' [required]') : '',
                $defaultParamValueTipOnDoc !== '' ? " = {$defaultParamValueTipOnDoc}" : ''
            );
            $paramsDeclarations[] = sprintf(
                '%s%s%s%s$%s%s',
                $nullTypeHint ? '?' : '',
                ($paramType === 'mixed' || $paramIsUnion) ? '' : "{$paramType} ",
                $variadic,
                $param->isPassedByReference() ? '&' : '',
                $param->getName(),
                $defaultParamValueTip !== '' ? " = {$defaultParamValueTip}" : ''
            );
        }
        $paramsDeclaration = implode(', ', $paramsDeclarations);
        // we can show more info on comment doc
        // if (strlen($paramsDeclaration) > 80) {
        //     $paramsDeclaration = $this::indent(
        //         "\n" . implode(",\n", $paramsDeclarations) . "\n",
        //         1
        //     );
        // }

        if ($function->hasReturnType()) {
            $returnType = $function->getReturnType();
            $returnTypeAllowNull = $returnType->allowsNull();
            /** @noinspection PhpPossiblePolymorphicInvocationInspection */
            $returnTypeName = $returnType->getName();
            if (class_exists($returnTypeName)) {
                if ($function instanceof ReflectionMethod && $returnTypeName === $function->getDeclaringClass()->getName()) {
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
        } else {
            $returnTypeAllowNull = false;
            $returnTypeName = 'mixed';
        }

        $isCtorOrDtor = $function instanceof ReflectionMethod && ($function->isConstructor() || $function->isDestructor());
        if (!$isCtorOrDtor) {
            $returnTypeNameInComment = $returnTypeName === 'self' ? '$this' : $returnTypeName;
            if ($returnTypeAllowNull) {
                $returnTypeNameInComment = "null|{$returnTypeNameInComment}";
            }
            $comment .= " * @return {$returnTypeNameInComment}\n";
            /*if ($returnTypeName !== 'void') {
                $body = " return \$GLOBALS[\"\\0fakeVariable\\0\"]; ";
            } else {
                $body = ' ';
            }*/
        }/* else {
            if ($function->getDeclaringClass()->getParentClass()) {
                if ($function->isConstructor()) {
                    $body = " parent::__construct(...\$GLOBALS[\"\\0fakeVariable\\0\"]); ";
                } else {
                    $body = " parent::__destruct(...\$GLOBALS[\"\\0fakeVariable\\0\"]); ";
                }
            } else {
                $body = ' ';
            }
        }*/
        $body = ' ';

        $declaration = $this->formatFunction(
            $function,
            $comment ? $comment = "/**\n{$comment} */\n" : '',
            $this->getDeclarationPrefix($function, false),
            $name,
            $paramsDeclaration,
            ($isCtorOrDtor || !$returnTypeName || $returnTypeName === 'mixed') ? '' : (': ' . ($returnTypeAllowNull ? '?' : '') . $returnTypeName),
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
        foreach ($class->getConstants() as $constantName => $constantValue) {
            $constantValue = is_numeric($constantValue) ? $constantValue : "'{$constantValue}'";
            $constantDeclarations[] = "public const {$constantName} = {$this::convertValueToString($constantValue)}";
        }

        $propertyDeclarations = [];
        foreach ($class->getProperties() as $property) {
            if ($property->getDeclaringClass()->getName() !== $class->getName()) {
                continue;
            }
            $propertyDeclarations[] = $this->getDeclarationPrefix($property, true) . '$' . $property->getName();
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

        $namespaceName = $namespaceName ? "namespace {$namespaceName}" : 'namespace';

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

        return
            "{$namespaceName}\n" .
            "{\n" .
            "    {$prefix}{$type} {$shortName}{$extends}{$implements}{$body}" .
            '}';
    }

    protected static function indent(string $content, int $level, ?string $eol = null): string
    {
        $eol = $eol ?? "\n";
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
