<?php
declare(strict_types=1);

namespace Swow\Mandelbrot\Gui;

use FFI;
use FFI\CData;
use Swow\Mandelbrot\Gui\Cocoa\ObjC;

/**
 * Cocoa backend loaded through FFI.
 *
 * AppKit requires the process main thread, so open() and pump() assert it.
 * The frame loop drives the event queue with nextEventMatchingMask:, therefore
 * no -[NSApplication run] loop is used. Buttons and text fields send their
 * actions to an Objective-C class created at runtime through ObjC, whose
 * method implementations are PHP closures.
 */
final class Cocoa implements Backend
{
    private const APPKIT_LIBRARY = '/System/Library/Frameworks/AppKit.framework/AppKit';
    private const FOUNDATION_LIBRARY = '/System/Library/Frameworks/Foundation.framework/Foundation';
    private const CORE_GRAPHICS_LIBRARY = '/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics';

    /*
    * AppKit and CoreGraphics constants used here
    *
    * kCGImageAlphaNoneSkipFirst is 6 and kCGBitmapByteOrder32Little is 2 << 12
    * NSApplicationActivationPolicyRegular is 0
    * NSWindowStyleMaskTitled, Closable and Miniaturizable are 1, 2 and 4
    * NSBackingStoreBuffered is 2, NSBezelStyleRounded is 1
    * NSImageScaleAxesIndependently is 1
    * NSModalResponseOK is 1, NSBitmapImageFileTypePNG is 4
    */
    private const PREVIEW_ZOOM = 4;
    private const BITMAP_INFO = 6 | 8192;
    private const RENDERING_INTENT_DEFAULT = 0;
    private const PNG_FILE_TYPE = 4;
    private const MODAL_RESPONSE_OK = 1;
    private const ACTIVATION_POLICY_REGULAR = 0;
    private const WINDOW_STYLE = 1 | 2 | 4;
    private const WINDOW_STYLE_RESIZABLE = 8;
    private const BACKING_BUFFERED = 2;
    private const BEZEL_STYLE_ROUNDED = 1;
    private const IMAGE_SCALE_FILL = 1;
    private const NSEVENT_MASK_ANY = -1;
    private const EVENT_TYPE_KEY_DOWN = 10;
    private const EVENT_MODIFIER_NONE = 0;
    private const KEY_CODE_RETURN = 36;
    private const FIELD_COMPONENTS = [
        're' => Layout::COMP_REAL,
        'im' => Layout::COMP_IMAG,
        'depth' => Layout::COMP_DEPTH,
    ];
    private const BUSY_COMPONENTS = [
        Layout::COMP_REAL,
        Layout::COMP_IMAG,
        Layout::COMP_DEPTH,
        Layout::COMP_ZOOM_OUT,
        Layout::COMP_ZOOM_IN,
        Layout::COMP_RESET,
        Layout::COMP_GO,
        Layout::COMP_SAVE,
        Layout::COMP_PAN_UP,
        Layout::COMP_PAN_DOWN,
        Layout::COMP_PAN_LEFT,
        Layout::COMP_PAN_RIGHT,
    ];

    private const APPKIT_CDEF = <<<'C'
    extern void *NSDefaultRunLoopMode;
    C;

    private const FOUNDATION_CDEF = <<<'C'
    extern double NSFoundationVersionNumber;
    C;

    private const CORE_GRAPHICS_CDEF = <<<'C'
    typedef struct CGImage *CGImageRef;
    typedef struct CGDataProvider *CGDataProviderRef;
    typedef struct CGColorSpace *CGColorSpaceRef;
    typedef unsigned long size_t;
    typedef unsigned int CGBitmapInfo;
    typedef int CGColorRenderingIntent;
    CGColorSpaceRef CGColorSpaceCreateDeviceRGB(void);
    void CGColorSpaceRelease(CGColorSpaceRef space);
    CGDataProviderRef CGDataProviderCreateWithData(void *info, const void *data,
        size_t size, void *releaseData);
    CGImageRef CGImageCreate(size_t width, size_t height, size_t bitsPerComponent,
        size_t bitsPerPixel, size_t bytesPerRow, CGColorSpaceRef space,
        CGBitmapInfo bitmapInfo, CGDataProviderRef provider, const double *decode,
        bool shouldInterpolate, CGColorRenderingIntent intent);
    void CGImageRelease(CGImageRef image);
    void CGDataProviderRelease(CGDataProviderRef provider);
    size_t CGImageGetWidth(CGImageRef image);
    size_t CGImageGetHeight(CGImageRef image);
    C;

    private ?ObjC $objc = null;
    private ?FFI $appkit = null;
    private ?FFI $foundation = null;
    private ?FFI $cg = null;

    private ?CData $app = null;
    private ?CData $window = null;
    private ?CData $contentView = null;
    private ?CData $layer = null;
    private ?CData $controller = null;
    private ?CData $placeholder = null;
    private ?CData $statusField = null;
    private ?CData $pool = null;
    private ?CData $distantPast = null;
    private ?CData $runLoopMode = null;
    private ?CData $defaultBackground = null;
    private ?CData $invalidBackground = null;

    /** @var array<string, CData> */
    private array $widgets = [];

    private ?CData $pixels = null;
    private ?CData $colorSpace = null;
    private ?CData $fullProvider = null;
    private ?CData $previewProvider = null;
    private ?CData $fullImage = null;
    private ?CData $previewImage = null;

    private int $imageWidth = 0;
    private int $imageHeight = 0;
    private int $previewWidth = 0;
    private int $previewHeight = 0;
    private int $fullRegionBytes = 0;
    private int $previewRegionBytes = 0;
    private int $contentWidth = 0;
    private int $contentHeight = 0;

    /** @var list<array<string, mixed>> */
    private array $events = [];

    private ?\Throwable $callbackError = null;

    private bool $placeholderVisible = false;
    private bool $busy = false;
    private bool $previewMode = true;
    private bool $dirty = false;

    /** Lazy libSystem handle for the direct usleep() call. */
    private ?FFI $libSystem = null;
    private bool $libSystemTried = false;

    public function open(int $width, int $height): void
    {
        if ($this->window !== null) {
            throw new \LogicException('Cocoa backend is already open');
        }

        $rects = Layout::resolve(Layout::tree(), Layout::metrics(), $width, $height);
        $canvas = $rects[Layout::COMP_CANVAS] ?? throw new \RuntimeException('layout has no canvas');
        $imageWidth = $canvas->w;
        $imageHeight = $canvas->h;
        if ($imageWidth < self::PREVIEW_ZOOM || $imageHeight < self::PREVIEW_ZOOM) {
            throw new \InvalidArgumentException(sprintf(
                'image size %dx%d is too small for a 1/%d preview',
                $imageWidth,
                $imageHeight,
                self::PREVIEW_ZOOM,
            ));
        }

        $this->objc = new ObjC();
        $this->appkit = FFI::cdef(self::APPKIT_CDEF, self::APPKIT_LIBRARY);
        $this->foundation = FFI::cdef(self::FOUNDATION_CDEF, self::FOUNDATION_LIBRARY);
        $this->cg = FFI::cdef(self::CORE_GRAPHICS_CDEF, self::CORE_GRAPHICS_LIBRARY);
        // Touch the symbol so a missing Foundation framework fails here
        $this->foundation->NSFoundationVersionNumber;

        $this->imageWidth = $imageWidth;
        $this->imageHeight = $imageHeight;
        $this->previewWidth = intdiv($imageWidth, self::PREVIEW_ZOOM);
        $this->previewHeight = intdiv($imageHeight, self::PREVIEW_ZOOM);
        $this->fullRegionBytes = $imageWidth * $imageHeight * 4;
        $this->previewRegionBytes = $this->previewWidth * $this->previewHeight * 4;
        $this->contentWidth = $width;
        $this->contentHeight = $height;

        try {
            $this->assertMainThread();
            $this->beginPool();
            $this->createApplication();
            $this->createController();
            $this->createWindow($width, $height);
            $this->createWidgets($rects);
            $this->createPlaceholder($canvas);
            $this->createImageBuffer();

            $this->buildImage(true);
            $this->buildImage(false);
            $this->defaultBackground = $this->retainColor('textBackgroundColor');
            $this->invalidBackground = $this->retainColor('redColor');
            $this->distantPast = $this->objc->send(
                ObjC::SIG_ID,
                $this->objc->send(ObjC::SIG_ID, $this->objc->classNamed('NSDate'), $this->objc->selector('distantPast')),
                $this->objc->selector('retain'),
            );
            $this->runLoopMode = $this->appkit->NSDefaultRunLoopMode;
            if ($this->runLoopMode === null) {
                $this->runLoopMode = $this->objc->string('NSDefaultRunLoopMode');
            }

            $this->presentWindow();
            $this->placeholderVisible = true;
            $this->busy = false;
            $this->previewMode = true;
            $this->drainPool();
        } catch (\Throwable $error) {
            $this->shutdown();
            throw $error;
        }
    }

    public function imageWidth(): int
    {
        $this->assertOpen();

        return $this->imageWidth;
    }

    public function imageHeight(): int
    {
        $this->assertOpen();

        return $this->imageHeight;
    }

    public function pump(): array
    {
        $this->assertOpen();
        $this->assertMainThread();
        $this->beginPool();

        $eventCount = 0;
        while (true) {
            $event = $this->objc->send(
                ObjC::SIG_EVENT,
                $this->app,
                $this->objc->selector('nextEventMatchingMask:untilDate:inMode:dequeue:'),
                self::NSEVENT_MASK_ANY,
                $this->distantPast,
                $this->runLoopMode,
                true,
            );
            if ($event === null) {
                break;
            }
            $this->objc->send(ObjC::SIG_VOID_OBJ, $this->app, $this->objc->selector('sendEvent:'), $event);
            $eventCount++;
        }

        $this->syncContentViewSize();

        if ($this->dirty) {
            $this->objc->send(ObjC::SIG_VOID, $this->app, $this->objc->selector('updateWindows'));
            $this->objc->send(ObjC::SIG_VOID, $this->window, $this->objc->selector('displayIfNeeded'));
            $this->objc->send(
                ObjC::SIG_VOID,
                $this->objc->classNamed('CATransaction'),
                $this->objc->selector('flush'),
            );
            $this->dirty = false;
        }

        if ($eventCount > 0) {
            $this->drainPool();
        }

        if ($this->callbackError !== null) {
            $error = $this->callbackError;
            $this->callbackError = null;
            throw new \RuntimeException('Cocoa callback failed: ' . $error->getMessage(), 0, $error);
        }

        $events = $this->events;
        $this->events = [];

        return $events;
    }

    public function setImageMode(bool $preview): void
    {
        $this->assertOpen();

        $this->previewMode = $preview;
        $this->showImage($preview);
    }

    public function setBusy(bool $busy): void
    {
        $this->assertOpen();
        if ($busy === $this->busy) {
            return;
        }
        $this->busy = $busy;

        foreach (self::BUSY_COMPONENTS as $component) {
            $this->objc->send(
                ObjC::SIG_VOID_BOOL,
                $this->widgets[$component],
                $this->objc->selector('setEnabled:'),
                !$busy,
            );
        }
        $this->dirty = true;
    }

    public function setTitle(string $title): void
    {
        $this->assertOpen();
        $this->objc->send(
            ObjC::SIG_VOID_OBJ,
            $this->window,
            $this->objc->selector('setTitle:'),
            $this->objc->string($title),
        );
    }

    public function wait(int $microseconds): void
    {
        $microseconds = max(1, $microseconds);

        if (!$this->libSystemTried) {
            $this->libSystemTried = true;
            try {
                $this->libSystem = FFI::cdef(
                    'int usleep(unsigned int usec);',
                    '/usr/lib/libSystem.B.dylib',
                );
            } catch (\Throwable) {
                $this->libSystem = null;
            }
        }

        if ($this->libSystem !== null) {
            try {
                if ($this->libSystem->usleep($microseconds) === 0) {
                    return;
                }
            } catch (\Throwable) {
            }
        }

        usleep($microseconds);
    }

    public function presentRows(string $bytes, int $rowBegin, int $rowEnd, int $width, bool $preview): void
    {
        $this->assertOpen();

        if ($this->placeholderVisible) {
            $this->objc->send(ObjC::SIG_VOID, $this->placeholder, $this->objc->selector('removeFromSuperview'));
            $this->placeholder = null;
            $this->placeholderVisible = false;
            $this->dirty = true;
        }

        $rows = $rowEnd - $rowBegin;
        if ($rowBegin < 0 || $rows < 1 || $width < 1) {
            throw new \InvalidArgumentException(sprintf(
                'invalid row band [%d, %d) with width %d',
                $rowBegin,
                $rowEnd,
                $width,
            ));
        }

        $expectedBytes = $rows * $width * 4;
        if (strlen($bytes) !== $expectedBytes) {
            throw new \InvalidArgumentException(sprintf(
                'pixel payload is %d bytes, expected %d for %d rows of %d pixels',
                strlen($bytes),
                $expectedBytes,
                $rows,
                $width,
            ));
        }

        if ($preview) {
            $expectedWidth = $this->previewWidth;
            $expectedHeight = $this->previewHeight;
        } else {
            $expectedWidth = $this->imageWidth;
            $expectedHeight = $this->imageHeight;
        }
        if ($width !== $expectedWidth || $rowEnd > $expectedHeight) {
            throw new \InvalidArgumentException(sprintf(
                'row band [%d, %d) x %d does not fit the %s image %dx%d',
                $rowBegin,
                $rowEnd,
                $width,
                $preview ? 'preview' : 'full',
                $expectedWidth,
                $expectedHeight,
            ));
        }

        $offset = $preview
            ? $this->fullRegionBytes + $rowBegin * $width * 4
            : $rowBegin * $width * 4;
        FFI::memcpy(FFI::addr($this->pixels[$offset]), $bytes, $expectedBytes);

        if ($preview === $this->previewMode) {
            $this->showImage($preview);
        }
    }

    public function setFields(string $re, string $im, string $depth): void
    {
        $this->assertOpen();

        $values = [
            Layout::COMP_REAL => $re,
            Layout::COMP_IMAG => $im,
            Layout::COMP_DEPTH => $depth,
        ];
        foreach ($values as $component => $value) {
            $this->objc->send(
                ObjC::SIG_VOID_OBJ,
                $this->widgets[$component],
                $this->objc->selector('setStringValue:'),
                $this->objc->string($value),
            );
        }
        $this->dirty = true;
    }

    public function markInvalid(?string $field, string $message): void
    {
        $this->assertOpen();

        if ($field === null) {
            foreach (self::FIELD_COMPONENTS as $component) {
                $this->objc->send(
                    ObjC::SIG_VOID_OBJ,
                    $this->widgets[$component],
                    $this->objc->selector('setBackgroundColor:'),
                    $this->defaultBackground,
                );
            }
        } else {
            $component = self::FIELD_COMPONENTS[$field]
                ?? throw new \InvalidArgumentException('unknown field: ' . $field);
            $this->objc->send(
                ObjC::SIG_VOID_OBJ,
                $this->widgets[$component],
                $this->objc->selector('setBackgroundColor:'),
                $this->invalidBackground,
            );
        }
        $this->dirty = true;

        $this->status($message);
    }

    public function status(string $text): void
    {
        $this->assertOpen();

        $this->objc->send(
            ObjC::SIG_VOID_OBJ,
            $this->statusField,
            $this->objc->selector('setStringValue:'),
            $this->objc->string($text),
        );
        $this->dirty = true;
    }

    public function askSavePath(string $default): ?string
    {
        $this->assertOpen();

        $panel = $this->objc->send(ObjC::SIG_ID, $this->objc->classNamed('NSSavePanel'), $this->objc->selector('savePanel'));
        $this->objc->send(
            ObjC::SIG_VOID_OBJ,
            $panel,
            $this->objc->selector('setTitle:'),
            $this->objc->string('Save Mandelbrot PNG'),
        );
        $this->objc->send(
            ObjC::SIG_VOID_OBJ,
            $panel,
            $this->objc->selector('setNameFieldStringValue:'),
            $this->objc->string($default),
        );
        $this->objc->send(
            ObjC::SIG_VOID_BOOL,
            $this->app,
            $this->objc->selector('activateIgnoringOtherApps:'),
            true,
        );

        if ($this->objc->send(ObjC::SIG_LONG, $panel, $this->objc->selector('runModal')) !== self::MODAL_RESPONSE_OK) {
            return null;
        }

        $url = $this->objc->send(ObjC::SIG_ID, $panel, $this->objc->selector('URL'));
        $path = $this->objc->send(ObjC::SIG_ID, $url, $this->objc->selector('path'));
        $value = (string) $this->objc->send(ObjC::SIG_CSTR, $path, $this->objc->selector('UTF8String'));
        if ($value === '') {
            return null;
        }
        if (!str_ends_with(strtolower($value), '.png')) {
            $value .= '.png';
        }

        return $value;
    }

    public function close(): void
    {
        $this->shutdown();
    }

    /**
     * Snapshot the content view through AppKit, for tests and screenshots.
     */
    public function snapshot(string $path): bool
    {
        $this->assertOpen();

        $this->objc->send(ObjC::SIG_VOID, $this->window, $this->objc->selector('displayIfNeeded'));
        $this->objc->send(
            ObjC::SIG_VOID,
            $this->objc->classNamed('CATransaction'),
            $this->objc->selector('flush'),
        );

        $rect = $this->makeRect(0, 0, $this->contentWidth, $this->contentHeight);
        $rep = $this->objc->send(
            ObjC::SIG_ID_CGRECT,
            $this->contentView,
            $this->objc->selector('bitmapImageRepForCachingDisplayInRect:'),
            $rect,
        );
        if ($rep === null) {
            return false;
        }
        $this->objc->send(
            ObjC::SIG_VOID_CGRECT_OBJ,
            $this->contentView,
            $this->objc->selector('cacheDisplayInRect:toBitmapImageRep:'),
            $rect,
            $rep,
        );
        $data = $this->objc->send(
            ObjC::SIG_ID_ULONG_OBJ,
            $rep,
            $this->objc->selector('representationUsingType:properties:'),
            self::PNG_FILE_TYPE,
            null,
        );
        if ($data === null) {
            return false;
        }

        return (bool) $this->objc->send(
            ObjC::SIG_BOOL_OBJ_BOOL,
            $data,
            $this->objc->selector('writeToFile:atomically:'),
            $this->objc->string($path),
            true,
        );
    }

    /**
     * Window title, client size, resizability and visibility.
     *
     * @return array{title: string, width: int, height: int, resizable: bool, visible: bool}
     */
    public function windowGeometry(): array
    {
        $this->assertOpen();

        $frame = $this->objc->send(ObjC::SIG_CGRECT, $this->contentView, $this->objc->selector('frame'));
        $title = (string) $this->objc->send(
            ObjC::SIG_CSTR,
            $this->objc->send(ObjC::SIG_ID, $this->window, $this->objc->selector('title')),
            $this->objc->selector('UTF8String'),
        );
        $style = $this->objc->send(ObjC::SIG_ULONG, $this->window, $this->objc->selector('styleMask'));

        return [
            'title' => $title,
            'width' => (int) round($frame->size->width),
            'height' => (int) round($frame->size->height),
            'resizable' => ($style & self::WINDOW_STYLE_RESIZABLE) !== 0,
            'visible' => (bool) $this->objc->send(ObjC::SIG_BOOL, $this->window, $this->objc->selector('isVisible')),
        ];
    }

    /**
     * Widget rect in the top-left based layout coordinates.
     *
     * @return array{x: int, y: int, w: int, h: int}
     */
    public function widgetFrame(string $component): array
    {
        $this->assertOpen();
        $widget = $this->widgets[$component]
            ?? throw new \InvalidArgumentException('unknown component: ' . $component);
        $frame = $this->objc->send(ObjC::SIG_CGRECT, $widget, $this->objc->selector('frame'));

        return [
            'x' => (int) round($frame->origin->x),
            'y' => (int) round($this->contentHeight - ($frame->origin->y + $frame->size->height)),
            'w' => (int) round($frame->size->width),
            'h' => (int) round($frame->size->height),
        ];
    }

    public function widgetEnabled(string $component): bool
    {
        $this->assertOpen();
        $widget = $this->widgets[$component]
            ?? throw new \InvalidArgumentException('unknown component: ' . $component);

        return (bool) $this->objc->send(ObjC::SIG_BOOL, $widget, $this->objc->selector('isEnabled'));
    }

    public function widgetAction(string $component): string
    {
        $this->assertOpen();
        $widget = $this->widgets[$component]
            ?? throw new \InvalidArgumentException('unknown component: ' . $component);
        $action = $this->objc->send(ObjC::SIG_ID, $widget, $this->objc->selector('action'));
        if ($action === null) {
            return '';
        }

        return $this->objc->selectorName($action);
    }

    public function widgetIsWired(string $component): bool
    {
        $this->assertOpen();
        $widget = $this->widgets[$component]
            ?? throw new \InvalidArgumentException('unknown component: ' . $component);
        $target = $this->objc->send(ObjC::SIG_ID, $widget, $this->objc->selector('target'));
        if ($target === null) {
            return false;
        }

        return (bool) $this->objc->send(
            ObjC::SIG_BOOL_OBJ,
            $target,
            $this->objc->selector('isEqual:'),
            $this->controller,
        );
    }

    public function fieldValue(string $component): string
    {
        $this->assertOpen();
        $widget = $this->widgets[$component]
            ?? throw new \InvalidArgumentException('unknown component: ' . $component);

        return (string) $this->objc->send(
            ObjC::SIG_CSTR,
            $this->objc->send(ObjC::SIG_ID, $widget, $this->objc->selector('stringValue')),
            $this->objc->selector('UTF8String'),
        );
    }

    /**
     * @return array{0: float, 1: float, 2: float, 3: float}
     */
    public function fieldBackground(string $component): array
    {
        $this->assertOpen();
        $widget = $this->widgets[$component]
            ?? throw new \InvalidArgumentException('unknown component: ' . $component);
        $color = $this->objc->send(ObjC::SIG_ID, $widget, $this->objc->selector('backgroundColor'));
        if ($color === null) {
            return [0.0, 0.0, 0.0, 0.0];
        }
        // Catalog colors do not expose components until converted
        $space = $this->objc->send(
            ObjC::SIG_ID,
            $this->objc->classNamed('NSColorSpace'),
            $this->objc->selector('deviceRGBColorSpace'),
        );
        $converted = $this->objc->send(
            ObjC::SIG_ID_OBJ,
            $color,
            $this->objc->selector('colorUsingColorSpace:'),
            $space,
        );
        if ($converted !== null) {
            $color = $converted;
        }

        return [
            (float) $this->objc->send(ObjC::SIG_DOUBLE, $color, $this->objc->selector('redComponent')),
            (float) $this->objc->send(ObjC::SIG_DOUBLE, $color, $this->objc->selector('greenComponent')),
            (float) $this->objc->send(ObjC::SIG_DOUBLE, $color, $this->objc->selector('blueComponent')),
            (float) $this->objc->send(ObjC::SIG_DOUBLE, $color, $this->objc->selector('alphaComponent')),
        ];
    }

    public function statusValue(): string
    {
        $this->assertOpen();

        return (string) $this->objc->send(
            ObjC::SIG_CSTR,
            $this->objc->send(ObjC::SIG_ID, $this->statusField, $this->objc->selector('stringValue')),
            $this->objc->selector('UTF8String'),
        );
    }

    public function placeholderShown(): bool
    {
        $this->assertOpen();

        return $this->placeholderVisible;
    }

    /**
     * Current layer contents size, preview or full image.
     *
     * @return array{0: int, 1: int}
     */
    public function imageSize(): array
    {
        $this->assertOpen();
        $image = $this->previewMode ? $this->previewImage : $this->fullImage;
        if ($image === null) {
            return [0, 0];
        }

        return [$this->cg->CGImageGetWidth($image), $this->cg->CGImageGetHeight($image)];
    }

    public function isPreviewMode(): bool
    {
        $this->assertOpen();

        return $this->previewMode;
    }

    public function performClick(string $component): void
    {
        $this->assertOpen();
        $widget = $this->widgets[$component]
            ?? throw new \InvalidArgumentException('unknown component: ' . $component);
        $this->objc->send(ObjC::SIG_VOID_OBJ, $widget, $this->objc->selector('performClick:'), null);
    }

    public function performFieldAction(string $component): bool
    {
        $this->assertOpen();
        $widget = $this->widgets[$component]
            ?? throw new \InvalidArgumentException('unknown component: ' . $component);
        $action = $this->objc->send(ObjC::SIG_ID, $widget, $this->objc->selector('action'));
        $target = $this->objc->send(ObjC::SIG_ID, $widget, $this->objc->selector('target'));

        return (bool) $this->objc->send(
            ObjC::SIG_BOOL_OBJ_OBJ,
            $widget,
            $this->objc->selector('sendAction:to:'),
            $action,
            $target,
        );
    }

    public function closeWindow(): void
    {
        $this->assertOpen();
        $this->objc->send(ObjC::SIG_VOID_OBJ, $this->window, $this->objc->selector('performClose:'), null);
    }

    /**
     * Post a synthetic Return key event to one field.
     *
     * The next pump() must dequeue it and let NSTextField run its action, which
     * exercises the Enter binding without a human at the keyboard.
     */
    public function postReturnKey(string $component): void
    {
        $this->assertOpen();
        $field = $this->widgets[$component]
            ?? throw new \InvalidArgumentException('unknown component: ' . $component);

        $this->objc->send(
            ObjC::SIG_VOID_BOOL,
            $this->app,
            $this->objc->selector('activateIgnoringOtherApps:'),
            true,
        );
        $this->objc->send(
            ObjC::SIG_VOID_OBJ,
            $this->window,
            $this->objc->selector('makeKeyAndOrderFront:'),
            null,
        );
        $this->objc->send(ObjC::SIG_VOID, $this->window, $this->objc->selector('makeKeyWindow'));
        $this->objc->send(
            ObjC::SIG_VOID_OBJ,
            $this->window,
            $this->objc->selector('makeFirstResponder:'),
            $field,
        );

        $location = $this->objc->new('CGPoint');
        $location->x = 0.0;
        $location->y = 0.0;
        $characters = $this->objc->string("\r");
        $event = $this->objc->send(
            ObjC::SIG_KEY_EVENT,
            $this->objc->classNamed('NSEvent'),
            $this->objc->selector(
                'keyEventWithType:location:modifierFlags:timestamp:windowNumber:context:'
                . 'characters:charactersIgnoringModifiers:isARepeat:keyCode:',
            ),
            self::EVENT_TYPE_KEY_DOWN,
            $location,
            self::EVENT_MODIFIER_NONE,
            0.0,
            $this->objc->send(ObjC::SIG_LONG, $this->window, $this->objc->selector('windowNumber')),
            null,
            $characters,
            $characters,
            false,
            self::KEY_CODE_RETURN,
        );
        if ($event === null) {
            throw new \RuntimeException('NSEvent creation failed');
        }
        $this->objc->send(
            ObjC::SIG_VOID_OBJ_BOOL,
            $this->app,
            $this->objc->selector('postEvent:atStart:'),
            $event,
            false,
        );
    }

    private function createApplication(): void
    {
        $app = $this->objc->send(
            ObjC::SIG_ID,
            $this->objc->classNamed('NSApplication'),
            $this->objc->selector('sharedApplication'),
        );
        if ($app === null) {
            throw new \RuntimeException('NSApplication sharedApplication failed');
        }
        $this->app = $app;

        $this->objc->send(
            ObjC::SIG_BOOL_LONG,
            $app,
            $this->objc->selector('setActivationPolicy:'),
            self::ACTIVATION_POLICY_REGULAR,
        );
        $this->objc->send(ObjC::SIG_VOID, $app, $this->objc->selector('finishLaunching'));
    }

    private function createController(): void
    {
        $name = $this->objc->className('MandelbrotGuiController');
        $class = $this->objc->newClass($name);

        $this->addAction($class, 'zoomOut:', 'zoomOut');
        $this->addAction($class, 'zoomIn:', 'zoomIn');
        $this->addAction($class, 'reset:', 'reset');
        $this->addAction($class, 'go:', 'apply');
        $this->addAction($class, 'save:', 'save');
        $this->addAction($class, 'quit:', 'quit');
        $this->addAction($class, 'panUp:', 'panUp');
        $this->addAction($class, 'panDown:', 'panDown');
        $this->addAction($class, 'panLeft:', 'panLeft');
        $this->addAction($class, 'panRight:', 'panRight');
        $this->addAction($class, 'windowWillClose:', 'quit');

        $this->objc->registerClass($class);

        $controller = $this->objc->send(
            ObjC::SIG_ID,
            $this->objc->classNamed($name),
            $this->objc->selector('alloc'),
        );
        $controller = $this->objc->send(ObjC::SIG_ID, $controller, $this->objc->selector('init'));
        if ($controller === null) {
            throw new \RuntimeException('controller initialization failed');
        }
        $this->controller = $controller;
    }

    /**
     * Attach one action method to the runtime class.
     *
     * Types "v@:@" describe a void method taking self, _cmd and one object,
     * which fits target/action and window delegate callbacks.
     */
    private function addAction(CData $class, string $selectorName, string $eventType): void
    {
        $this->objc->addMethod(
            $class,
            $selectorName,
            function ($self, $cmd, $sender) use ($eventType): void {
                try {
                    if ($eventType === 'apply') {
                        $this->events[] = [
                            'type' => 'apply',
                            're' => $this->fieldValue(Layout::COMP_REAL),
                            'im' => $this->fieldValue(Layout::COMP_IMAG),
                            'depth' => $this->fieldValue(Layout::COMP_DEPTH),
                        ];
                    } else {
                        $this->events[] = ['type' => $eventType];
                    }
                    $this->dirty = true;
                } catch (\Throwable $error) {
                    // Exceptions cannot cross the C callback boundary
                    $this->callbackError = $error;
                }
            },
            'v@:@',
        );
    }

    private function createWindow(int $width, int $height): void
    {
        $frame = $this->makeRect(0, 0, $width, $height);
        $window = $this->objc->send(ObjC::SIG_ID, $this->objc->classNamed('NSWindow'), $this->objc->selector('alloc'));
        $window = $this->objc->send(
            ObjC::SIG_ID_CGRECT_ULONG_ULONG_BOOL,
            $window,
            $this->objc->selector('initWithContentRect:styleMask:backing:defer:'),
            $frame,
            self::WINDOW_STYLE,
            self::BACKING_BUFFERED,
            false,
        );
        if ($window === null) {
            throw new \RuntimeException('NSWindow initialization failed');
        }
        $this->window = $window;

        $this->objc->send(
            ObjC::SIG_VOID_OBJ,
            $window,
            $this->objc->selector('setTitle:'),
            $this->objc->string(Layout::TITLE),
        );
        $this->objc->send(ObjC::SIG_VOID_BOOL, $window, $this->objc->selector('setReleasedWhenClosed:'), false);
        $this->objc->send(ObjC::SIG_VOID_OBJ, $window, $this->objc->selector('setDelegate:'), $this->controller);
        $this->objc->send(ObjC::SIG_VOID, $window, $this->objc->selector('center'));

        $contentView = $this->objc->send(ObjC::SIG_ID, $window, $this->objc->selector('contentView'));
        if ($contentView === null) {
            throw new \RuntimeException('window has no content view');
        }
        $this->contentView = $contentView;
        $this->objc->send(ObjC::SIG_VOID_BOOL, $contentView, $this->objc->selector('setWantsLayer:'), true);

        $layer = $this->objc->send(ObjC::SIG_ID, $contentView, $this->objc->selector('layer'));
        if ($layer === null) {
            throw new \RuntimeException('content view has no layer');
        }
        $this->layer = $layer;
        /*
        * The image sits at the bottom of the content view and is scaled by
        * contentsScale: 1/4 for the preview, 1 for the full frame. The strip
        * above it stays the layer background colour and hosts the toolbar
        */
        $this->objc->send(
            ObjC::SIG_VOID_OBJ,
            $layer,
            $this->objc->selector('setContentsGravity:'),
            $this->objc->string('bottomLeft'),
        );
        $this->objc->send(
            ObjC::SIG_VOID_OBJ,
            $layer,
            $this->objc->selector('setMagnificationFilter:'),
            $this->objc->string('nearest'),
        );
        $this->objc->send(
            ObjC::SIG_VOID_OBJ,
            $layer,
            $this->objc->selector('setBackgroundColor:'),
            $this->objc->send(
                ObjC::SIG_ID,
                $this->objc->send(ObjC::SIG_ID, $this->objc->classNamed('NSColor'), $this->objc->selector('whiteColor')),
                $this->objc->selector('CGColor'),
            ),
        );
    }

    /**
     * @param array<string, Rect> $rects
     */
    private function createWidgets(array $rects): void
    {
        foreach (self::leaves(Layout::tree()) as $leaf) {
            if ($leaf->leaf === LeafKind::Spacer || $leaf->key === '') {
                continue;
            }
            $rect = $rects[$leaf->key] ?? throw new \RuntimeException(
                'missing layout rect for ' . $leaf->key,
            );
            $frame = $this->makeRect(
                $rect->x,
                $this->contentHeight - ($rect->y + $rect->h),
                $rect->w,
                $rect->h,
            );

            switch ($leaf->leaf) {
                case LeafKind::Label:
                    $label = $this->objc->send(
                        ObjC::SIG_ID,
                        $this->objc->classNamed('NSTextField'),
                        $this->objc->selector('alloc'),
                    );
                    $label = $this->objc->send(
                        ObjC::SIG_ID_CGRECT,
                        $label,
                        $this->objc->selector('initWithFrame:'),
                        $frame,
                    );
                    $this->objc->send(
                        ObjC::SIG_VOID_OBJ,
                        $label,
                        $this->objc->selector('setStringValue:'),
                        $this->objc->string($leaf->text),
                    );
                    $this->objc->send(ObjC::SIG_VOID_BOOL, $label, $this->objc->selector('setBezeled:'), false);
                    $this->objc->send(ObjC::SIG_VOID_BOOL, $label, $this->objc->selector('setDrawsBackground:'), false);
                    $this->objc->send(ObjC::SIG_VOID_BOOL, $label, $this->objc->selector('setEditable:'), false);
                    $this->objc->send(ObjC::SIG_VOID_BOOL, $label, $this->objc->selector('setSelectable:'), false);
                    if ($leaf->component === Layout::COMP_STATUS) {
                        $this->objc->send(
                            ObjC::SIG_VOID_OBJ,
                            $label,
                            $this->objc->selector('setFont:'),
                            $this->fontOfSize(11.0),
                        );
                        $this->statusField = $label;
                        $this->widgets[Layout::COMP_STATUS] = $label;
                    }
                    $this->objc->send(ObjC::SIG_VOID_OBJ, $this->contentView, $this->objc->selector('addSubview:'), $label);
                    $this->objc->send(ObjC::SIG_VOID, $label, $this->objc->selector('release'));
                    break;

                case LeafKind::Edit:
                    $component = $leaf->component ?? throw new \RuntimeException(
                        'edit leaf without a component',
                    );
                    $edit = $this->objc->send(
                        ObjC::SIG_ID,
                        $this->objc->classNamed('NSTextField'),
                        $this->objc->selector('alloc'),
                    );
                    $edit = $this->objc->send(
                        ObjC::SIG_ID_CGRECT,
                        $edit,
                        $this->objc->selector('initWithFrame:'),
                        $frame,
                    );
                    $this->objc->send(ObjC::SIG_VOID_BOOL, $edit, $this->objc->selector('setEditable:'), true);
                    $this->objc->send(ObjC::SIG_VOID_BOOL, $edit, $this->objc->selector('setSelectable:'), true);
                    $this->objc->send(ObjC::SIG_VOID_BOOL, $edit, $this->objc->selector('setBezeled:'), true);
                    $this->objc->send(ObjC::SIG_VOID_BOOL, $edit, $this->objc->selector('setDrawsBackground:'), true);
                    $this->objc->send(
                        ObjC::SIG_VOID_OBJ,
                        $edit,
                        $this->objc->selector('setFont:'),
                        $this->fontOfSize(13.0),
                    );
                    $this->objc->send(ObjC::SIG_VOID_OBJ, $edit, $this->objc->selector('setTarget:'), $this->controller);
                    $this->objc->send(ObjC::SIG_VOID_OBJ, $edit, $this->objc->selector('setAction:'), $this->objc->selector('go:'));
                    $this->objc->send(ObjC::SIG_VOID_OBJ, $this->contentView, $this->objc->selector('addSubview:'), $edit);
                    $this->widgets[$component] = $edit;
                    $this->objc->send(ObjC::SIG_VOID, $edit, $this->objc->selector('release'));
                    break;

                case LeafKind::Button:
                    $component = $leaf->component ?? throw new \RuntimeException(
                        'button leaf without a component',
                    );
                    $button = $this->objc->send(
                        ObjC::SIG_ID,
                        $this->objc->classNamed('NSButton'),
                        $this->objc->selector('alloc'),
                    );
                    $button = $this->objc->send(
                        ObjC::SIG_ID_CGRECT,
                        $button,
                        $this->objc->selector('initWithFrame:'),
                        $frame,
                    );
                    $this->objc->send(
                        ObjC::SIG_VOID_OBJ,
                        $button,
                        $this->objc->selector('setTitle:'),
                        $this->objc->string($leaf->text),
                    );
                    $this->objc->send(
                        ObjC::SIG_VOID_ULONG,
                        $button,
                        $this->objc->selector('setBezelStyle:'),
                        self::BEZEL_STYLE_ROUNDED,
                    );
                    $this->objc->send(
                        ObjC::SIG_VOID_OBJ,
                        $button,
                        $this->objc->selector('setFont:'),
                        $this->fontOfSize(12.0),
                    );
                    $this->objc->send(ObjC::SIG_VOID_OBJ, $button, $this->objc->selector('setTarget:'), $this->controller);
                    $this->objc->send(
                        ObjC::SIG_VOID_OBJ,
                        $button,
                        $this->objc->selector('setAction:'),
                        $this->objc->selector(self::buttonAction($component)),
                    );
                    $this->objc->send(ObjC::SIG_VOID_OBJ, $this->contentView, $this->objc->selector('addSubview:'), $button);
                    $this->widgets[$component] = $button;
                    $this->objc->send(ObjC::SIG_VOID, $button, $this->objc->selector('release'));
                    break;
            }
        }
    }

    private static function buttonAction(string $component): string
    {
        return match ($component) {
            Layout::COMP_ZOOM_OUT => 'zoomOut:',
            Layout::COMP_ZOOM_IN => 'zoomIn:',
            Layout::COMP_RESET => 'reset:',
            Layout::COMP_GO => 'go:',
            Layout::COMP_SAVE => 'save:',
            Layout::COMP_QUIT => 'quit:',
            Layout::COMP_PAN_UP => 'panUp:',
            Layout::COMP_PAN_DOWN => 'panDown:',
            Layout::COMP_PAN_LEFT => 'panLeft:',
            Layout::COMP_PAN_RIGHT => 'panRight:',
            default => throw new \InvalidArgumentException('unknown button: ' . $component),
        };
    }

    private function createPlaceholder(Rect $canvas): void
    {
        $path = Layout::placeholderPath();
        $image = $this->objc->send(ObjC::SIG_ID, $this->objc->classNamed('NSImage'), $this->objc->selector('alloc'));
        $image = $this->objc->send(
            ObjC::SIG_ID_OBJ,
            $image,
            $this->objc->selector('initWithContentsOfFile:'),
            $this->objc->string($path),
        );
        if ($image === null) {
            throw new \RuntimeException('cannot load placeholder image: ' . $path);
        }

        $view = $this->objc->send(ObjC::SIG_ID, $this->objc->classNamed('NSImageView'), $this->objc->selector('alloc'));
        $view = $this->objc->send(
            ObjC::SIG_ID_CGRECT,
            $view,
            $this->objc->selector('initWithFrame:'),
            $this->makeRect(
                $canvas->x,
                $this->contentHeight - ($canvas->y + $canvas->h),
                $canvas->w,
                $canvas->h,
            ),
        );
        $this->objc->send(ObjC::SIG_VOID_OBJ, $view, $this->objc->selector('setImage:'), $image);
        $this->objc->send(ObjC::SIG_VOID, $image, $this->objc->selector('release'));
        $this->objc->send(
            ObjC::SIG_VOID_ULONG,
            $view,
            $this->objc->selector('setImageScaling:'),
            self::IMAGE_SCALE_FILL,
        );
        $this->objc->send(ObjC::SIG_VOID_OBJ, $this->contentView, $this->objc->selector('addSubview:'), $view);
        $this->objc->send(ObjC::SIG_VOID, $view, $this->objc->selector('release'));
        $this->placeholder = $view;
    }

    private function createImageBuffer(): void
    {
        $this->colorSpace = $this->cg->CGColorSpaceCreateDeviceRGB();
        if ($this->colorSpace === null) {
            throw new \RuntimeException('CGColorSpaceCreateDeviceRGB failed');
        }

        $total = $this->fullRegionBytes + $this->previewRegionBytes;
        $this->pixels = $this->objc->new('unsigned char[' . $total . ']');
        FFI::memset(FFI::addr($this->pixels[0]), 0, $total);

        $fullProvider = $this->cg->CGDataProviderCreateWithData(
            null,
            FFI::addr($this->pixels[0]),
            $this->fullRegionBytes,
            null,
        );
        $previewProvider = $this->cg->CGDataProviderCreateWithData(
            null,
            FFI::addr($this->pixels[$this->fullRegionBytes]),
            $this->previewRegionBytes,
            null,
        );
        if ($fullProvider === null || $previewProvider === null) {
            throw new \RuntimeException('CGDataProviderCreateWithData failed');
        }
        $this->fullProvider = $fullProvider;
        $this->previewProvider = $previewProvider;
    }

    private function buildImage(bool $preview): CData
    {
        if ($preview) {
            $old = $this->previewImage;
            $image = $this->cg->CGImageCreate(
                $this->previewWidth,
                $this->previewHeight,
                8,
                32,
                $this->previewWidth * 4,
                $this->colorSpace,
                self::BITMAP_INFO,
                $this->previewProvider,
                null,
                false,
                self::RENDERING_INTENT_DEFAULT,
            );
            $this->previewImage = $image;
        } else {
            $old = $this->fullImage;
            $image = $this->cg->CGImageCreate(
                $this->imageWidth,
                $this->imageHeight,
                8,
                32,
                $this->imageWidth * 4,
                $this->colorSpace,
                self::BITMAP_INFO,
                $this->fullProvider,
                null,
                false,
                self::RENDERING_INTENT_DEFAULT,
            );
            $this->fullImage = $image;
        }
        if ($image === null) {
            throw new \RuntimeException('CGImageCreate failed');
        }
        if ($old !== null) {
            $this->cg->CGImageRelease($old);
        }

        return $image;
    }

    private function showImage(bool $preview): void
    {
        $image = $this->buildImage($preview);
        $this->objc->send(
            ObjC::SIG_VOID_DOUBLE,
            $this->layer,
            $this->objc->selector('setContentsScale:'),
            $preview ? 1.0 / self::PREVIEW_ZOOM : 1.0,
        );
        $this->objc->send(ObjC::SIG_VOID_OBJ, $this->layer, $this->objc->selector('setContents:'), $image);
        $this->dirty = true;
    }

    private function syncContentViewSize(): void
    {
        if ($this->contentView === null) {
            return;
        }
        $bounds = $this->objc->send(ObjC::SIG_CGRECT, $this->contentView, $this->objc->selector('bounds'));
        $width = (int) round($bounds->size->width);
        $height = (int) round($bounds->size->height);
        if ($width === $this->contentWidth && $height === $this->contentHeight) {
            return;
        }

        $this->contentWidth = $width;
        $this->contentHeight = $height;
        // The window is fixed, but a screen change can still resize the view
        if ($this->fullImage !== null) {
            $this->showImage($this->previewMode);
        }
    }

    private function presentWindow(): void
    {
        $this->objc->send(ObjC::SIG_VOID_OBJ, $this->window, $this->objc->selector('makeKeyAndOrderFront:'), null);
        $this->objc->send(
            ObjC::SIG_VOID_BOOL,
            $this->app,
            $this->objc->selector('activateIgnoringOtherApps:'),
            true,
        );
        $this->objc->send(ObjC::SIG_VOID, $this->window, $this->objc->selector('displayIfNeeded'));
        $this->objc->send(
            ObjC::SIG_VOID,
            $this->objc->classNamed('CATransaction'),
            $this->objc->selector('flush'),
        );
    }

    private function retainColor(string $selectorName): CData
    {
        $color = $this->objc->send(
            ObjC::SIG_ID,
            $this->objc->classNamed('NSColor'),
            $this->objc->selector($selectorName),
        );
        if ($color === null) {
            throw new \RuntimeException('NSColor lookup failed: ' . $selectorName);
        }
        $retained = $this->objc->send(ObjC::SIG_ID, $color, $this->objc->selector('retain'));
        if ($retained === null) {
            throw new \RuntimeException('NSColor retain failed: ' . $selectorName);
        }

        return $retained;
    }

    private function fontOfSize(float $size): CData
    {
        $font = $this->objc->send(
            ObjC::SIG_ID_DOUBLE,
            $this->objc->classNamed('NSFont'),
            $this->objc->selector('systemFontOfSize:'),
            $size,
        );
        if ($font === null) {
            throw new \RuntimeException('NSFont lookup failed');
        }

        return $font;
    }

    private function beginPool(): void
    {
        if ($this->pool === null) {
            $this->pool = $this->objc->send(
                ObjC::SIG_ID,
                $this->objc->classNamed('NSAutoreleasePool'),
                $this->objc->selector('new'),
            );
        }
    }

    private function drainPool(): void
    {
        if ($this->pool !== null) {
            $this->objc->send(ObjC::SIG_VOID, $this->pool, $this->objc->selector('drain'));
            $this->pool = null;
        }
    }

    private function assertMainThread(): void
    {
        if ($this->objc === null) {
            return;
        }
        if (!$this->objc->send(ObjC::SIG_BOOL, $this->objc->classNamed('NSThread'), $this->objc->selector('isMainThread'))) {
            throw new \RuntimeException('Cocoa backend must run on the process main thread');
        }
    }

    private function assertOpen(): void
    {
        if ($this->window === null) {
            throw new \LogicException('Cocoa backend is not open');
        }
    }

    private function makeRect(int $x, int $y, int $width, int $height): CData
    {
        $rect = $this->objc->new('CGRect');
        $rect->origin->x = $x;
        $rect->origin->y = $y;
        $rect->size->width = $width;
        $rect->size->height = $height;

        return $rect;
    }

    /**
     * @return list<Node>
     */
    private static function leaves(Node $node): array
    {
        if ($node->kind === NodeKind::Leaf) {
            return [$node];
        }

        $leaves = [];
        foreach ($node->children as $child) {
            foreach (self::leaves($child) as $leaf) {
                $leaves[] = $leaf;
            }
        }

        return $leaves;
    }

    private function shutdown(): void
    {
        if ($this->objc !== null) {
            try {
                if ($this->placeholder !== null) {
                    $this->objc->send(ObjC::SIG_VOID, $this->placeholder, $this->objc->selector('removeFromSuperview'));
                }
                if ($this->window !== null) {
                    $this->objc->send(ObjC::SIG_VOID_OBJ, $this->window, $this->objc->selector('setDelegate:'), null);
                    $this->objc->send(ObjC::SIG_VOID, $this->window, $this->objc->selector('close'));
                    $this->objc->send(ObjC::SIG_VOID, $this->window, $this->objc->selector('release'));
                }
                if ($this->controller !== null) {
                    $this->objc->send(ObjC::SIG_VOID, $this->controller, $this->objc->selector('release'));
                }
                if ($this->distantPast !== null) {
                    $this->objc->send(ObjC::SIG_VOID, $this->distantPast, $this->objc->selector('release'));
                }
                if ($this->defaultBackground !== null) {
                    $this->objc->send(ObjC::SIG_VOID, $this->defaultBackground, $this->objc->selector('release'));
                }
                if ($this->invalidBackground !== null) {
                    $this->objc->send(ObjC::SIG_VOID, $this->invalidBackground, $this->objc->selector('release'));
                }
            } catch (\Throwable) {
                // Closing must stay safe after a partial open
            }
            $this->drainPool();
        }

        if ($this->cg !== null) {
            if ($this->fullImage !== null) {
                $this->cg->CGImageRelease($this->fullImage);
            }
            if ($this->previewImage !== null) {
                $this->cg->CGImageRelease($this->previewImage);
            }
            if ($this->fullProvider !== null) {
                $this->cg->CGDataProviderRelease($this->fullProvider);
            }
            if ($this->previewProvider !== null) {
                $this->cg->CGDataProviderRelease($this->previewProvider);
            }
            if ($this->colorSpace !== null) {
                $this->cg->CGColorSpaceRelease($this->colorSpace);
            }
        }

        $this->app = null;
        $this->window = null;
        $this->contentView = null;
        $this->layer = null;
        $this->controller = null;
        $this->placeholder = null;
        $this->statusField = null;
        $this->distantPast = null;
        $this->runLoopMode = null;
        $this->defaultBackground = null;
        $this->invalidBackground = null;
        $this->pixels = null;
        $this->colorSpace = null;
        $this->fullProvider = null;
        $this->previewProvider = null;
        $this->fullImage = null;
        $this->previewImage = null;
        $this->widgets = [];
        $this->events = [];
        $this->callbackError = null;
        $this->imageWidth = 0;
        $this->imageHeight = 0;
        $this->previewWidth = 0;
        $this->previewHeight = 0;
        $this->fullRegionBytes = 0;
        $this->previewRegionBytes = 0;
        $this->contentWidth = 0;
        $this->contentHeight = 0;
        $this->placeholderVisible = false;
        $this->busy = false;
        $this->previewMode = true;
        $this->dirty = false;
        $this->objc = null;
        $this->appkit = null;
        $this->foundation = null;
        $this->cg = null;
    }
}
