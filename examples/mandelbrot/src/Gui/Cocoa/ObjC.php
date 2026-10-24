<?php

declare(strict_types=1);

namespace Swow\Mandelbrot\Gui\Cocoa;

use Closure;
use FFI;
use FFI\CData;
use RuntimeException;

/**
 * Objective-C runtime access through FFI.
 *
 * Covers the libobjc handle, message sends, runtime class creation and
 * NSString conversion. objc_msgSend is variadic in C and PHP FFI cannot call a
 * variadic symbol, so each selector shape has an exact prototype that the
 * symbol is cast to once and cached.
 */
final class ObjC
{
    private const LIBRARY = '/usr/lib/libobjc.A.dylib';

    /*
    * Exact objc_msgSend prototypes, one per selector shape
    */
    public const SIG_ID = 'void *(*)(void *, void *)';
    public const SIG_ID_CHAR = 'void *(*)(void *, void *, const char *)';
    public const SIG_ID_DOUBLE = 'void *(*)(void *, void *, double)';
    public const SIG_ID_OBJ = 'void *(*)(void *, void *, void *)';
    public const SIG_ID_CGRECT = 'void *(*)(void *, void *, CGRect)';
    public const SIG_ID_ULONG_OBJ = 'void *(*)(void *, void *, unsigned long, void *)';
    public const SIG_ID_CGRECT_ULONG_ULONG_BOOL = 'void *(*)(void *, void *, CGRect, unsigned long, unsigned long, bool)';
    public const SIG_VOID = 'void (*)(void *, void *)';
    public const SIG_VOID_OBJ = 'void (*)(void *, void *, void *)';
    public const SIG_VOID_BOOL = 'void (*)(void *, void *, bool)';
    public const SIG_VOID_DOUBLE = 'void (*)(void *, void *, double)';
    public const SIG_VOID_ULONG = 'void (*)(void *, void *, unsigned long)';
    public const SIG_VOID_CGRECT_OBJ = 'void (*)(void *, void *, CGRect, void *)';
    public const SIG_BOOL = 'bool (*)(void *, void *)';
    public const SIG_BOOL_OBJ = 'bool (*)(void *, void *, void *)';
    public const SIG_BOOL_LONG = 'bool (*)(void *, void *, long)';
    public const SIG_BOOL_OBJ_OBJ = 'bool (*)(void *, void *, void *, void *)';
    public const SIG_BOOL_OBJ_BOOL = 'bool (*)(void *, void *, void *, bool)';
    public const SIG_LONG = 'long (*)(void *, void *)';
    public const SIG_ULONG = 'unsigned long (*)(void *, void *)';
    public const SIG_DOUBLE = 'double (*)(void *, void *)';
    public const SIG_CSTR = 'const char *(*)(void *, void *)';
    public const SIG_CGRECT = 'CGRect (*)(void *, void *)';
    public const SIG_EVENT = 'void *(*)(void *, void *, unsigned long, void *, void *, bool)';
    public const SIG_VOID_OBJ_BOOL = 'void (*)(void *, void *, void *, bool)';
    public const SIG_KEY_EVENT = 'void *(*)(void *, void *, unsigned long, CGPoint, unsigned long, double, long, void *, void *, void *, bool, unsigned short)';

    private const CDEF = <<<'C'
    typedef struct objc_class *Class;
    typedef struct objc_selector *SEL;
    typedef void *id;
    typedef struct { double x; double y; } CGPoint;
    typedef struct { double width; double height; } CGSize;
    typedef struct { CGPoint origin; CGSize size; } CGRect;
    typedef void (*MandelAction)(void *self, void *cmd, void *arg);
    typedef struct { MandelAction fn; } MandelActionHolder;
    void *objc_getClass(const char *name);
    void *sel_registerName(const char *name);
    const char *sel_getName(void *name);
    void *objc_msgSend(void);
    void *objc_allocateClassPair(void *superclass, const char *name, unsigned long extraBytes);
    void objc_registerClassPair(void *cls);
    bool class_addMethod(void *cls, void *name, MandelAction imp, const char *types);
    C;

    /** Class name serial, unique for the life of the process. */
    private static int $classSerial = 0;

    private FFI $ffi;

    /** @var array<string, CData> */
    private array $selectors = [];

    /** @var array<string, CData> */
    private array $sends = [];

    /** @var list<CData> Runtime keeps only IMP pointers, these own the closures. */
    private array $impHolders = [];

    public function __construct()
    {
        $this->ffi = FFI::cdef(self::CDEF, self::LIBRARY);
    }

    /** Send one message through the cached cast for $prototype. */
    public function send(string $prototype, mixed ...$arguments): mixed
    {
        $function = $this->sends[$prototype] ??= $this->ffi->cast(
            $prototype,
            $this->ffi->objc_msgSend,
        );

        return $function(...$arguments);
    }

    public function selector(string $name): CData
    {
        return $this->selectors[$name] ??= $this->ffi->sel_registerName($name);
    }

    public function selectorName(CData $selector): string
    {
        return (string) $this->ffi->sel_getName($selector);
    }

    public function classNamed(string $name): CData
    {
        $class = $this->ffi->objc_getClass($name);
        if ($class === null) {
            throw new RuntimeException('Objective-C class not found: ' . $name);
        }

        return $class;
    }

    /** Allocate CData for a type declared by the cdef above. */
    public function new(string $type): CData
    {
        return $this->ffi->new($type);
    }

    /** Runtime class name with a serial suffix, never reused in the process. */
    public function className(string $prefix): string
    {
        return $prefix . (++self::$classSerial);
    }

    /** Allocate a class pair, visible to the runtime after registerClass(). */
    public function newClass(string $name, string $superclass = 'NSObject'): CData
    {
        $class = $this->ffi->objc_allocateClassPair($this->classNamed($superclass), $name, 0);
        if ($class === null) {
            throw new RuntimeException('objc_allocateClassPair failed for ' . $name);
        }

        return $class;
    }

    /**
     * Add one method whose implementation is a PHP closure.
     *
     * The runtime stores only the function pointer, so the holder keeps the
     * closure alive until this object is dropped.
     */
    public function addMethod(CData $class, string $selectorName, Closure $implementation, string $types): void
    {
        $holder = $this->ffi->new('MandelActionHolder');
        $holder->fn = $implementation;
        if (!$this->ffi->class_addMethod($class, $this->selector($selectorName), $holder->fn, $types)) {
            throw new RuntimeException('class_addMethod failed: ' . $selectorName);
        }

        $this->impHolders[] = $holder;
    }

    public function registerClass(CData $class): void
    {
        $this->ffi->objc_registerClassPair($class);
    }

    public function string(string $value): CData
    {
        $string = $this->send(
            self::SIG_ID_CHAR,
            $this->classNamed('NSString'),
            $this->selector('stringWithUTF8String:'),
            $value,
        );
        if ($string === null) {
            throw new RuntimeException('NSString creation failed');
        }

        return $string;
    }
}
