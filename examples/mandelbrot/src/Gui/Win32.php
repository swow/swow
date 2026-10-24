<?php

declare(strict_types=1);

namespace Swow\Mandelbrot\Gui;

use FFI;
use FFI\CData;
use InvalidArgumentException;
use LogicException;
use RuntimeException;
use Swow\Mandelbrot\Gui\Win32\Dialog;
use Swow\Mandelbrot\Gui\Win32\Png;
use Swow\Mandelbrot\Gui\Win32\Text;
use Throwable;

/**
 * Native Win32 backend loaded through FFI.
 *
 * Window creation and the message pump must run on the same thread. The frame
 * loop calls pump(), so no blocking message loop runs here. A PHP closure
 * serves as WndProc; it only queues intent, as the Tk backend does.
 *
 * DPI: the process asks for DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 before
 * the window exists, so Windows does not virtualize coordinates. One layout
 * DIP maps to one physical pixel, and the client area is exactly 1280x720
 * physical pixels at any display scale. A DPI-unaware host would instead get
 * a scaled, blurry window.
 *
 * Image: one FFI allocation holds the full BGRA frame at offset 0 and the
 * quarter size preview frame after it. While the placeholder is visible the
 * full area holds the decoded assets/placeholder.png, which is also the first
 * image painted. WM_PAINT blits through StretchDIBits with a top-down
 * BITMAPINFO; the preview is stretched 4x with COLORONCOLOR to match Tk's
 * zoomed photo.
 *
 * Single-line EDIT controls do not notify their parent when Enter is pressed,
 * so Enter does not apply the fields. Use the Go button.
 *
 * Set MANDELBROT_TRACE=1 to write pump calls slower than 50 ms to stderr,
 * naming the slowest dispatched message or the drain loop. Set
 * MANDELBROT_SPIN=1 to make wait() return immediately and spin instead of
 * sleeping; this is an escape hatch for hosts where even 1 ms waits hurt.
 * winmm timeBeginPeriod(1) is used to make 1 ms waits real; if it is
 * unavailable the backend continues without it.
 *
 * The PNG decoder, the UTF-16 text helpers and the save dialog live in the
 * Win32 subnamespace; this class keeps the window, the message pump, the
 * widgets and the painting.
 *
 * Requires 64-bit PHP, FFI, zlib, and Windows 10 version 1703 or newer for
 * SetProcessDpiAwarenessContext.
 */
final class Win32 implements Backend
{
    private const WINDOW_CLASS = 'SwowMandelbrotWindow';
    private const PREVIEW_ZOOM = 4;
    private const TRACE_SLOW_MS = 50.0;
    private const BITMAPINFOHEADER_SIZE = 40;

    private const INVALID_COLOR = 0x006666FF;

    /* PM_REMOVE */
    private const PM_REMOVE = 0x0001;
    /* QS_ALLINPUT */
    private const QS_ALLINPUT = 0x04FF;
    /* MWMO_INPUTAVAILABLE */
    private const MWMO_INPUTAVAILABLE = 0x0004;
    private const WAIT_FAILED = 0xFFFFFFFF;
    private const TIMER_PERIOD_MS = 1;
    private const SW_SHOW = 5;
    private const BN_CLICKED = 0;
    private const WM_QUIT = 0x0012;
    private const SWP_NOMOVE = 0x0002;
    private const SWP_NOZORDER = 0x0004;
    private const SWP_NOACTIVATE = 0x0010;
    private const DIB_RGB_COLORS = 0;
    private const SRCCOPY = 0x00CC0020;
    private const COLORONCOLOR = 3;
    private const TRANSPARENT = 1;

    private const WM_DESTROY = 0x0002;
    private const WM_PAINT = 0x000F;
    private const WM_CLOSE = 0x0010;
    private const WM_SETFONT = 0x0030;
    private const WM_COMMAND = 0x0111;
    private const WM_CTLCOLOREDIT = 0x0133;
    private const WM_CTLCOLORSTATIC = 0x0138;

    /* WS_CHILD | WS_VISIBLE */
    private const CONTROL_STYLE = 0x50000000;
    /* WS_CHILD | WS_VISIBLE | WS_TABSTOP */
    private const INPUT_STYLE = 0x50010000;
    private const WS_BORDER = 0x00800000;
    /* WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN */
    private const WINDOW_STYLE = 0x02CA0000;
    private const ES_AUTOHSCROLL = 0x0080;
    private const SS_CENTERIMAGE = 0x00000200;
    private const SS_NOPREFIX = 0x00000080;

    /* DEFAULT_GUI_FONT */
    private const STOCK_FONT = 17;
    /* COLOR_BTNFACE */
    private const COLOR_BTNFACE = 15;
    /* IDC_ARROW */
    private const CURSOR_ARROW = 32512;
    /* DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2, (HANDLE)-4 */
    private const DPI_PER_MONITOR_V2 = -4;

    private const ID_REAL = 101;
    private const ID_IMAG = 102;
    private const ID_DEPTH = 103;
    private const ID_ZOOM_OUT = 104;
    private const ID_ZOOM_IN = 105;
    private const ID_RESET = 106;
    private const ID_GO = 107;
    private const ID_SAVE = 108;
    private const ID_QUIT = 109;
    private const ID_PAN_UP = 110;
    private const ID_PAN_DOWN = 111;
    private const ID_PAN_LEFT = 112;
    private const ID_PAN_RIGHT = 113;

    private const FIELD_COMPONENTS = [
        're' => Layout::COMP_REAL,
        'im' => Layout::COMP_IMAG,
        'depth' => Layout::COMP_DEPTH,
    ];

    private const CONTROL_IDS = [
        Layout::COMP_REAL => self::ID_REAL,
        Layout::COMP_IMAG => self::ID_IMAG,
        Layout::COMP_DEPTH => self::ID_DEPTH,
        Layout::COMP_ZOOM_OUT => self::ID_ZOOM_OUT,
        Layout::COMP_ZOOM_IN => self::ID_ZOOM_IN,
        Layout::COMP_RESET => self::ID_RESET,
        Layout::COMP_GO => self::ID_GO,
        Layout::COMP_SAVE => self::ID_SAVE,
        Layout::COMP_QUIT => self::ID_QUIT,
        Layout::COMP_PAN_UP => self::ID_PAN_UP,
        Layout::COMP_PAN_DOWN => self::ID_PAN_DOWN,
        Layout::COMP_PAN_LEFT => self::ID_PAN_LEFT,
        Layout::COMP_PAN_RIGHT => self::ID_PAN_RIGHT,
    ];

    private const USER32_CDEF = <<<'C'
    typedef intptr_t (*MandelWndProc)(intptr_t hwnd, unsigned int msg, uintptr_t wparam, intptr_t lparam);
    typedef struct {
        MandelWndProc proc;
    } MandelWndProcHolder;
    typedef struct {
        unsigned int cbSize;
        unsigned int style;
        MandelWndProc lpfnWndProc;
        int32_t cbClsExtra;
        int32_t cbWndExtra;
        intptr_t hInstance;
        intptr_t hIcon;
        intptr_t hCursor;
        intptr_t hbrBackground;
        uint16_t *lpszMenuName;
        uint16_t *lpszClassName;
        intptr_t hIconSm;
    } WNDCLASSEXW;
    typedef struct {
        int32_t left;
        int32_t top;
        int32_t right;
        int32_t bottom;
    } RECT;
    typedef struct {
        intptr_t hwnd;
        unsigned int message;
        uintptr_t wParam;
        intptr_t lParam;
        uint32_t time;
        int32_t ptX;
        int32_t ptY;
        uint32_t lPrivate;
    } MSG;
    typedef struct {
        intptr_t hdc;
        int32_t fErase;
        RECT rcPaint;
        int32_t fRestore;
        int32_t fIncUpdate;
        uint8_t rgbReserved[32];
    } PAINTSTRUCT;
    int SetProcessDpiAwarenessContext(intptr_t context);
    int SetProcessDPIAware(void);
    intptr_t LoadCursorW(intptr_t hInstance, intptr_t cursorName);
    int RegisterClassExW(WNDCLASSEXW *wndClass);
    int UnregisterClassW(uint16_t *className, intptr_t hInstance);
    intptr_t CreateWindowExW(unsigned int exStyle, uint16_t *className,
        uint16_t *windowName, unsigned int style, int x, int y, int width,
        int height, intptr_t parent, intptr_t menu, intptr_t hInstance,
        intptr_t param);
    int DestroyWindow(intptr_t hwnd);
    int ShowWindow(intptr_t hwnd, int cmd);
    int UpdateWindow(intptr_t hwnd);
    int GetClientRect(intptr_t hwnd, RECT *rect);
    int GetWindowRect(intptr_t hwnd, RECT *rect);
    int SetWindowPos(intptr_t hwnd, intptr_t insertAfter, int x, int y,
        int width, int height, unsigned int flags);
    int AdjustWindowRectEx(RECT *rect, unsigned int style, int menu,
        unsigned int exStyle);
    int GetWindowTextW(intptr_t hwnd, uint16_t *buffer, int maxChars);
    int SetWindowTextW(intptr_t hwnd, uint16_t *text);
    int EnableWindow(intptr_t hwnd, int enable);
    int IsWindowEnabled(intptr_t hwnd);
    intptr_t GetSysColorBrush(int index);
    unsigned int MsgWaitForMultipleObjectsEx(unsigned int count,
        const void *handles, unsigned int milliseconds, unsigned int wakeMask,
        unsigned int flags);
    int PeekMessageW(MSG *msg, intptr_t hwnd, unsigned int min,
        unsigned int max, unsigned int remove);
    int TranslateMessage(MSG *msg);
    intptr_t DispatchMessageW(MSG *msg);
    intptr_t DefWindowProcW(intptr_t hwnd, unsigned int msg,
        uintptr_t wparam, intptr_t lparam);
    void PostQuitMessage(int code);
    int InvalidateRect(intptr_t hwnd, RECT *rect, int erase);
    intptr_t SendMessageW(intptr_t hwnd, unsigned int msg,
        uintptr_t wparam, intptr_t lparam);
    intptr_t BeginPaint(intptr_t hwnd, PAINTSTRUCT *paint);
    int EndPaint(intptr_t hwnd, PAINTSTRUCT *paint);
    C;

    private const GDI32_CDEF = <<<'C'
    typedef struct {
        uint32_t biSize;
        int32_t biWidth;
        int32_t biHeight;
        uint16_t biPlanes;
        uint16_t biBitCount;
        uint32_t biCompression;
        uint32_t biSizeImage;
        int32_t biXPelsPerMeter;
        int32_t biYPelsPerMeter;
        uint32_t biClrUsed;
        uint32_t biClrImportant;
    } BITMAPINFOHEADER;
    typedef struct {
        BITMAPINFOHEADER bmiHeader;
        uint32_t bmiColors[1];
    } BITMAPINFO;
    int StretchDIBits(intptr_t hdc, int xDest, int yDest, int wDest, int hDest,
        int xSrc, int ySrc, int wSrc, int hSrc, const void *bits,
        BITMAPINFO *info, unsigned int usage, uint32_t rop);
    int SetStretchBltMode(intptr_t hdc, int mode);
    int SetBkMode(intptr_t hdc, int mode);
    uint32_t SetBkColor(intptr_t hdc, uint32_t color);
    intptr_t CreateSolidBrush(uint32_t color);
    int DeleteObject(intptr_t object);
    intptr_t GetStockObject(int object);
    C;

    private const KERNEL32_CDEF = <<<'C'
    intptr_t GetModuleHandleW(uint16_t *name);
    uint32_t GetLastError(void);
    void Sleep(unsigned int milliseconds);
    C;

    private const WINMM_CDEF = <<<'C'
    unsigned int timeBeginPeriod(unsigned int period);
    unsigned int timeEndPeriod(unsigned int period);
    C;

    private ?FFI $user32 = null;
    private ?FFI $gdi32 = null;
    private ?FFI $kernel32 = null;
    private ?FFI $winmm = null;
    private ?Text $text = null;
    private ?Dialog $dialog = null;
    private bool $timerPeriodActive = false;

    private int $hwnd = 0;
    private int $hInstance = 0;
    private bool $classRegistered = false;
    private int $font = 0;
    private int $backgroundBrush = 0;
    private int $invalidBrush = 0;

    private ?CData $className = null;
    private ?CData $title = null;
    private ?CData $wndProcHolder = null;
    private ?CData $pixels = null;
    private ?CData $bitmapInfo = null;
    private ?CData $message = null;
    private ?CData $paintStruct = null;
    private ?CData $invalidateRect = null;

    private int $imageX = 0;
    private int $imageY = 0;
    private int $imageWidth = 0;
    private int $imageHeight = 0;
    private int $previewWidth = 0;
    private int $previewHeight = 0;
    private int $previewOffset = 0;
    private int $fullBytes = 0;
    private int $previewBytes = 0;

    private bool $preview = false;
    private bool $placeholderVisible = false;
    private bool $busy = false;
    private ?string $invalidField = null;

    /** @var array<string, int> */
    private array $widgets = [];

    /** @var list<int> */
    private array $busyWidgets = [];

    private int $statusWidget = 0;

    /** @var list<array<string, mixed>> */
    private array $events = [];

    private bool $quitQueued = false;
    private ?Throwable $failure = null;
    private bool $trace = false;
    private bool $spin = false;
    private int $dispatchedMessages = 0;

    public function open(int $width, int $height): void
    {
        if ($this->user32 !== null) {
            throw new LogicException('Win32 backend is already open');
        }
        if (PHP_INT_SIZE < 8) {
            throw new RuntimeException('Win32 backend requires 64-bit PHP');
        }
        if (!extension_loaded('ffi')) {
            throw new RuntimeException('FFI extension is not loaded');
        }
        if (!extension_loaded('zlib')) {
            throw new RuntimeException('zlib extension is not loaded');
        }

        $rects = Layout::resolve(Layout::tree(), Layout::metrics(), $width, $height);
        $canvas = $rects[Layout::COMP_CANVAS];
        $imageWidth = $canvas->w;
        $imageHeight = $canvas->h;
        $this->trace = (getenv('MANDELBROT_TRACE') ?: '') === '1';
        $this->spin = (getenv('MANDELBROT_SPIN') ?: '') === '1';
        if ($imageWidth < self::PREVIEW_ZOOM || $imageHeight < self::PREVIEW_ZOOM) {
            throw new InvalidArgumentException(sprintf(
                'image size %dx%d is too small for a 1/%d preview',
                $imageWidth,
                $imageHeight,
                self::PREVIEW_ZOOM,
            ));
        }

        try {
            $this->user32 = FFI::cdef(self::USER32_CDEF, 'user32.dll');
            $this->gdi32 = FFI::cdef(self::GDI32_CDEF, 'gdi32.dll');
            $this->kernel32 = FFI::cdef(self::KERNEL32_CDEF, 'kernel32.dll');
            $this->dialog = new Dialog();

            $this->enableDpiAwareness();

            $hInstance = $this->kernel32->GetModuleHandleW(null);
            if ($hInstance === 0) {
                throw new RuntimeException('GetModuleHandleW failed: ' . $this->lastError());
            }
            $this->hInstance = $hInstance;

            $this->imageX = $canvas->x;
            $this->imageY = $canvas->y;
            $this->imageWidth = $imageWidth;
            $this->imageHeight = $imageHeight;
            $this->previewWidth = intdiv($imageWidth, self::PREVIEW_ZOOM);
            $this->previewHeight = intdiv($imageHeight, self::PREVIEW_ZOOM);
            $this->fullBytes = $imageWidth * $imageHeight * 4;
            $this->previewBytes = $this->previewWidth * $this->previewHeight * 4;
            $this->previewOffset = $this->fullBytes;

            $this->font = $this->gdi32->GetStockObject(self::STOCK_FONT);
            $this->backgroundBrush = $this->user32->GetSysColorBrush(self::COLOR_BTNFACE);
            $this->invalidBrush = $this->gdi32->CreateSolidBrush(self::INVALID_COLOR);
            if ($this->invalidBrush === 0) {
                throw new RuntimeException('CreateSolidBrush failed: ' . $this->lastError());
            }

            $this->text = new Text($this->user32);
            $this->className = $this->text->wide(self::WINDOW_CLASS);
            $this->title = $this->text->wide(Layout::TITLE);
            $this->message = $this->user32->new('MSG');
            $this->paintStruct = $this->user32->new('PAINTSTRUCT');
            $this->invalidateRect = $this->user32->new('RECT');

            $this->createWindow($width, $height);
            $this->enableTimerPeriod();
            $this->createWidgets($rects);
            $this->createBitmap();

            $this->pixels = $this->user32->new(
                'unsigned char[' . ($this->fullBytes + $this->previewBytes) . ']',
            );
            FFI::memset($this->pixels, 0, $this->fullBytes + $this->previewBytes);
            $this->loadPlaceholder();

            $this->user32->ShowWindow($this->hwnd, self::SW_SHOW);
            $this->user32->UpdateWindow($this->hwnd);
        } catch (Throwable $error) {
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
        $this->throwFailure();

        $message = $this->message;
        $trace = $this->trace;
        $pumpStart = $trace ? microtime(true) : 0.0;
        $slowestMs = 0.0;
        $slowestLabel = 'drain';
        $quit = false;

        while (true) {
            if ($trace) {
                $operationStart = microtime(true);
            }
            $hasMessage = $this->user32->PeekMessageW(
                FFI::addr($message),
                0,
                0,
                0,
                self::PM_REMOVE,
            ) !== 0;
            if ($trace) {
                $elapsed = (microtime(true) - $operationStart) * 1000.0;
                if ($elapsed > $slowestMs) {
                    $slowestMs = $elapsed;
                    $slowestLabel = 'drain';
                }
            }
            if (!$hasMessage) {
                break;
            }
            if ($message->message === self::WM_QUIT) {
                $quit = true;
                break;
            }
            if ($trace) {
                $operationStart = microtime(true);
            }
            $this->user32->TranslateMessage(FFI::addr($message));
            $this->user32->DispatchMessageW(FFI::addr($message));
            $this->dispatchedMessages++;
            if ($trace) {
                $elapsed = (microtime(true) - $operationStart) * 1000.0;
                if ($elapsed > $slowestMs) {
                    $slowestMs = $elapsed;
                    $slowestLabel = 'dispatch ' . self::messageLabel($message->message);
                }
            }
        }
        if ($quit) {
            $this->queueQuit();
        }

        $events = $this->events;
        $this->events = [];
        $this->quitQueued = false;
        $this->throwFailure();

        if ($trace) {
            $elapsed = (microtime(true) - $pumpStart) * 1000.0;
            if ($elapsed > self::TRACE_SLOW_MS) {
                fwrite(STDERR, sprintf(
                    "mandelbrot: trace: pump %.1f ms slowest=%s %.1f ms\n",
                    $elapsed,
                    $slowestLabel,
                    $slowestMs,
                ));
            }
        }

        return $events;
    }

    /**
     * Sleep the calling thread until input arrives or the interval expires.
     *
     * MsgWaitForMultipleObjectsEx waits without dispatching, so the next
     * pump() sees the input. This bypasses swow's usleep() override. With
     * MANDELBROT_SPIN=1 the call returns immediately and the loop spins.
     */
    public function wait(int $microseconds): void
    {
        if ($this->spin || $microseconds <= 0) {
            return;
        }

        $milliseconds = max(1, (int) ceil($microseconds / 1000));

        try {
            if ($this->hwnd === 0
                || $this->user32 === null
                || $this->kernel32 === null
            ) {
                if ($this->kernel32 !== null) {
                    $this->kernel32->Sleep($milliseconds);
                } else {
                    usleep($microseconds);
                }

                return;
            }

            $result = $this->user32->MsgWaitForMultipleObjectsEx(
                0,
                null,
                $milliseconds,
                self::QS_ALLINPUT,
                0,
            );
            if (($result & 0xFFFFFFFF) === self::WAIT_FAILED) {
                $this->kernel32->Sleep($milliseconds);
            }
        } catch (Throwable) {
            /*
            * wait() must never throw; a skipped wait only delays the next
            * pump by one loop iteration
            */
        }
    }

    public function setImageMode(bool $preview): void
    {
        $this->assertOpen();

        $this->preview = $preview;
        $this->invalidateImage();
    }

    public function setBusy(bool $busy): void
    {
        $this->assertOpen();
        if ($busy === $this->busy) {
            return;
        }
        $this->busy = $busy;

        foreach ($this->busyWidgets as $widget) {
            $this->user32->EnableWindow($widget, $busy ? 0 : 1);
        }
    }

    public function presentRows(string $bytes, int $rowBegin, int $rowEnd, int $width, bool $preview): void
    {
        $this->assertOpen();

        $droppedPlaceholder = false;
        if ($this->placeholderVisible) {
            $this->placeholderVisible = false;
            $droppedPlaceholder = true;
            /*
            * The full region holds the placeholder. Clear it so a later mode
            * switch cannot show stale decorations before full rows arrive.
            */
            FFI::memset($this->pixels, 0, $this->fullBytes);
            $this->invalidateImage();
        }

        $rows = $rowEnd - $rowBegin;
        if ($rowBegin < 0 || $rows < 1 || $width < 1) {
            throw new InvalidArgumentException(sprintf(
                'invalid row band [%d, %d) with width %d',
                $rowBegin,
                $rowEnd,
                $width,
            ));
        }

        $expectedBytes = $rows * $width * 4;
        if (strlen($bytes) !== $expectedBytes) {
            throw new InvalidArgumentException(sprintf(
                'pixel payload is %d bytes, expected %d for %d rows of %d pixels',
                strlen($bytes),
                $expectedBytes,
                $rows,
                $width,
            ));
        }

        $expectedWidth = $preview ? $this->previewWidth : $this->imageWidth;
        $expectedHeight = $preview ? $this->previewHeight : $this->imageHeight;
        if ($width !== $expectedWidth || $rowEnd > $expectedHeight) {
            throw new InvalidArgumentException(sprintf(
                'row band [%d, %d) x %d does not fit the %s image %dx%d',
                $rowBegin,
                $rowEnd,
                $width,
                $preview ? 'preview' : 'full',
                $expectedWidth,
                $expectedHeight,
            ));
        }

        $offset = ($preview ? $this->previewOffset : 0) + $rowBegin * $width * 4;
        FFI::memcpy(FFI::addr($this->pixels[$offset]), $bytes, $expectedBytes);
        if (!$droppedPlaceholder) {
            $this->invalidateRows($rowBegin, $rowEnd, $preview);
        }
    }

    public function setFields(string $re, string $im, string $depth): void
    {
        $this->assertOpen();

        $this->text->setWindowText($this->widgets[Layout::COMP_REAL], $re);
        $this->text->setWindowText($this->widgets[Layout::COMP_IMAG], $im);
        $this->text->setWindowText($this->widgets[Layout::COMP_DEPTH], $depth);
    }

    public function markInvalid(?string $field, string $message): void
    {
        $this->assertOpen();

        if ($field === null) {
            $this->invalidField = null;
            foreach (self::FIELD_COMPONENTS as $component) {
                $this->invalidateWidget($this->widgets[$component]);
            }
        } else {
            if (!isset(self::FIELD_COMPONENTS[$field])) {
                throw new InvalidArgumentException('unknown field: ' . $field);
            }
            $this->invalidField = $field;
            $this->invalidateWidget($this->widgets[self::FIELD_COMPONENTS[$field]]);
        }

        $this->status($message);
    }

    public function status(string $text): void
    {
        $this->assertOpen();

        $this->text->setWindowText($this->statusWidget, $text);
    }

    /**
     * Replace the window title. No-op once the window is gone.
     */
    public function setTitle(string $title): void
    {
        if ($this->hwnd === 0 || $this->user32 === null) {
            return;
        }

        try {
            $this->text->setWindowText($this->hwnd, $title);
        } catch (Throwable) {
            /*
            * Title updates are informational; never fail the frame loop
            */
        }
    }

    public function askSavePath(string $default): ?string
    {
        $this->assertOpen();

        return $this->dialog->askSavePath($this->hwnd, $this->hInstance, $default);
    }

    public function close(): void
    {
        $this->shutdown();
    }

    /**
     * Native window handle. Test and diagnostic use only.
     */
    public function windowHandle(): int
    {
        $this->assertOpen();

        return $this->hwnd;
    }

    /**
     * Number of messages pump() has dispatched since open. Diagnostic only.
     */
    public function dispatchedMessages(): int
    {
        $this->assertOpen();

        return $this->dispatchedMessages;
    }

    /**
     * Client size in physical pixels. Test and diagnostic use only.
     *
     * @return array{0: int, 1: int}
     */
    public function clientSize(): array
    {
        $this->assertOpen();

        $rect = $this->user32->new('RECT');
        if ($this->user32->GetClientRect($this->hwnd, FFI::addr($rect)) === 0) {
            throw new RuntimeException('GetClientRect failed: ' . $this->lastError());
        }

        return [$rect->right - $rect->left, $rect->bottom - $rect->top];
    }

    /**
     * Current window title. Test and diagnostic use only.
     */
    public function windowTitle(): string
    {
        $this->assertOpen();

        return $this->text->windowText($this->hwnd);
    }

    /**
     * Whether the placeholder still covers the image area. Test use only.
     */
    public function placeholderVisible(): bool
    {
        $this->assertOpen();

        return $this->placeholderVisible;
    }

    /**
     * Whether the preview layer is selected. Test use only.
     */
    public function imageMode(): bool
    {
        $this->assertOpen();

        return $this->preview;
    }

    /**
     * Current text of one entry. Test use only.
     */
    public function fieldText(string $field): string
    {
        $this->assertOpen();
        if (!isset(self::FIELD_COMPONENTS[$field])) {
            throw new InvalidArgumentException('unknown field: ' . $field);
        }

        return $this->text->windowText($this->widgets[self::FIELD_COMPONENTS[$field]]);
    }

    /**
     * Current text of the status line. Test use only.
     */
    public function statusText(): string
    {
        $this->assertOpen();

        return $this->text->windowText($this->statusWidget);
    }

    /**
     * Whether one native control accepts input. Test use only.
     */
    public function widgetEnabled(string $component): bool
    {
        $this->assertOpen();
        if (!isset($this->widgets[$component])) {
            throw new InvalidArgumentException('unknown component: ' . $component);
        }

        return $this->user32->IsWindowEnabled($this->widgets[$component]) !== 0;
    }

    /**
     * Field marked invalid, or null. Test use only.
     */
    public function invalidField(): ?string
    {
        $this->assertOpen();

        return $this->invalidField;
    }

    /**
     * Pack a WM_COMMAND wParam from a control id and notification code.
     */
    public static function commandWParam(int $id, int $code): int
    {
        return (($code & 0xFFFF) << 16) | ($id & 0xFFFF);
    }

    public static function commandId(int $wparam): int
    {
        return $wparam & 0xFFFF;
    }

    public static function commandCode(int $wparam): int
    {
        return ($wparam >> 16) & 0xFFFF;
    }

    private static function messageLabel(int $message): string
    {
        return match ($message) {
            self::WM_PAINT => 'WM_PAINT',
            self::WM_COMMAND => 'WM_COMMAND',
            self::WM_CTLCOLOREDIT => 'WM_CTLCOLOREDIT',
            self::WM_CTLCOLORSTATIC => 'WM_CTLCOLORSTATIC',
            default => sprintf('0x%04X', $message),
        };
    }

    private function enableDpiAwareness(): void
    {
        /*
        * Per-monitor v2 keeps layout coordinates in physical pixels. The call
        * fails when a manifest already set awareness, which is harmless.
        */
        if ($this->user32->SetProcessDpiAwarenessContext(self::DPI_PER_MONITOR_V2) === 0) {
            $this->user32->SetProcessDPIAware();
        }
    }

    /**
     * Ask winmm for 1 ms timer resolution.
     *
     * Without it, Sleep and MsgWait timeouts below the 15.6 ms system tick
     * block for the whole tick. Failure is harmless and only traced.
     */
    private function enableTimerPeriod(): void
    {
        $failure = null;
        $result = 0;

        try {
            $this->winmm = FFI::cdef(self::WINMM_CDEF, 'winmm.dll');
            $result = $this->winmm->timeBeginPeriod(self::TIMER_PERIOD_MS);
            $this->timerPeriodActive = $result === 0;
        } catch (Throwable $error) {
            $this->winmm = null;
            $this->timerPeriodActive = false;
            $failure = $error;
        }

        if (!$this->trace) {
            return;
        }

        try {
            if ($failure !== null) {
                fwrite(
                    STDERR,
                    'mandelbrot: trace: winmm unavailable: '
                        . str_replace(["\r", "\n"], ' ', $failure->getMessage()) . "\n",
                );
            } elseif (!$this->timerPeriodActive) {
                fwrite(STDERR, sprintf(
                    "mandelbrot: trace: timeBeginPeriod(%d) failed: %u\n",
                    self::TIMER_PERIOD_MS,
                    $result,
                ));
            }
        } catch (Throwable) {
        }
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
            $rect = $rects[$leaf->key] ?? throw new RuntimeException(
                'missing layout rect for ' . $leaf->key,
            );

            switch ($leaf->leaf) {
                case LeafKind::Label:
                    $style = self::CONTROL_STYLE | self::SS_CENTERIMAGE;
                    if ($leaf->component === Layout::COMP_STATUS) {
                        $this->statusWidget = $this->createControl(
                            'STATIC',
                            '',
                            $style | self::SS_NOPREFIX,
                            $rect,
                            0,
                        );
                    } else {
                        $this->createControl('STATIC', $leaf->text, $style, $rect, 0);
                    }
                    break;

                case LeafKind::Edit:
                    /*
                    * A single-line EDIT does not notify its parent of Enter,
                    * so applying the fields needs the Go button
                    */
                    $component = $leaf->component ?? throw new RuntimeException(
                        'edit leaf without a component',
                    );
                    $handle = $this->createControl(
                        'EDIT',
                        '',
                        self::INPUT_STYLE | self::WS_BORDER | self::ES_AUTOHSCROLL,
                        $rect,
                        self::CONTROL_IDS[$component],
                    );
                    $this->widgets[$component] = $handle;
                    break;

                case LeafKind::Button:
                    $component = $leaf->component ?? throw new RuntimeException(
                        'button leaf without a component',
                    );
                    $handle = $this->createControl(
                        'BUTTON',
                        $leaf->text,
                        self::INPUT_STYLE,
                        $rect,
                        self::CONTROL_IDS[$component],
                    );
                    $this->widgets[$component] = $handle;
                    break;
            }
        }

        $this->busyWidgets = [
            $this->widgets[Layout::COMP_REAL],
            $this->widgets[Layout::COMP_IMAG],
            $this->widgets[Layout::COMP_DEPTH],
            $this->widgets[Layout::COMP_ZOOM_OUT],
            $this->widgets[Layout::COMP_ZOOM_IN],
            $this->widgets[Layout::COMP_RESET],
            $this->widgets[Layout::COMP_GO],
            $this->widgets[Layout::COMP_SAVE],
            $this->widgets[Layout::COMP_PAN_UP],
            $this->widgets[Layout::COMP_PAN_DOWN],
            $this->widgets[Layout::COMP_PAN_LEFT],
            $this->widgets[Layout::COMP_PAN_RIGHT],
        ];
    }

    private function createControl(string $class, string $text, int $style, Rect $rect, int $id): int
    {
        $className = $this->text->wide($class);
        $windowName = $this->text->wide($text);
        $handle = $this->user32->CreateWindowExW(
            0,
            $className,
            $windowName,
            $style,
            $rect->x,
            $rect->y,
            $rect->w,
            $rect->h,
            $this->hwnd,
            $id,
            $this->hInstance,
            0,
        );
        if ($handle === 0) {
            throw new RuntimeException(
                'CreateWindowExW failed for ' . $class . ': ' . $this->lastError(),
            );
        }
        $this->user32->SendMessageW($handle, self::WM_SETFONT, $this->font, 1);

        return $handle;
    }

    private function createWindow(int $width, int $height): void
    {
        $this->wndProcHolder = $this->user32->new('MandelWndProcHolder');
        $this->wndProcHolder->proc = function (
            int $hwnd,
            int $msg,
            int $wparam,
            int $lparam,
        ): int {
            try {
                return $this->handleMessage($hwnd, $msg, $wparam, $lparam);
            } catch (Throwable $error) {
                /*
                * Exceptions must not cross the FFI callback boundary. Keep the
                * first failure and report it from pump() on the PHP side.
                */
                if ($this->failure === null) {
                    $this->failure = $error;
                }

                return $this->user32->DefWindowProcW($hwnd, $msg, $wparam, $lparam);
            }
        };

        $windowClass = $this->user32->new('WNDCLASSEXW');
        $windowClass->cbSize = FFI::sizeof($windowClass);
        $windowClass->style = 0;
        $windowClass->lpfnWndProc = $this->wndProcHolder->proc;
        $windowClass->cbClsExtra = 0;
        $windowClass->cbWndExtra = 0;
        $windowClass->hInstance = $this->hInstance;
        $windowClass->hIcon = 0;
        $windowClass->hCursor = $this->user32->LoadCursorW(0, self::CURSOR_ARROW);
        $windowClass->hbrBackground = $this->backgroundBrush;
        $windowClass->lpszMenuName = null;
        $windowClass->lpszClassName = $this->user32->cast('uint16_t *', $this->className);
        $windowClass->hIconSm = 0;

        if ($this->user32->RegisterClassExW(FFI::addr($windowClass)) === 0) {
            throw new RuntimeException('RegisterClassExW failed: ' . $this->lastError());
        }
        $this->classRegistered = true;

        $rect = $this->user32->new('RECT');
        $rect->left = 0;
        $rect->top = 0;
        $rect->right = $width;
        $rect->bottom = $height;
        if ($this->user32->AdjustWindowRectEx(
            FFI::addr($rect),
            self::WINDOW_STYLE,
            0,
            0,
        ) === 0) {
            throw new RuntimeException('AdjustWindowRectEx failed: ' . $this->lastError());
        }

        $hwnd = $this->user32->CreateWindowExW(
            0,
            $this->className,
            $this->title,
            self::WINDOW_STYLE,
            100,
            100,
            $rect->right - $rect->left,
            $rect->bottom - $rect->top,
            0,
            0,
            $this->hInstance,
            0,
        );
        if ($hwnd === 0) {
            throw new RuntimeException('CreateWindowExW failed: ' . $this->lastError());
        }
        $this->hwnd = $hwnd;
        $this->fitClientSize($width, $height);
    }

    /**
     * Correct the client size after creation.
     *
     * AdjustWindowRectEx uses the thread DPI, while the window can open on a
     * monitor with another DPI. Measuring the real client area and resizing
     * once keeps the 1280x720 contract on every monitor.
     */
    private function fitClientSize(int $width, int $height): void
    {
        $client = $this->user32->new('RECT');
        $window = $this->user32->new('RECT');
        if ($this->user32->GetClientRect($this->hwnd, FFI::addr($client)) === 0
            || $this->user32->GetWindowRect($this->hwnd, FFI::addr($window)) === 0
        ) {
            return;
        }

        $clientWidth = $client->right - $client->left;
        $clientHeight = $client->bottom - $client->top;
        if ($clientWidth === $width && $clientHeight === $height) {
            return;
        }

        $this->user32->SetWindowPos(
            $this->hwnd,
            0,
            0,
            0,
            ($window->right - $window->left) + ($width - $clientWidth),
            ($window->bottom - $window->top) + ($height - $clientHeight),
            self::SWP_NOMOVE | self::SWP_NOZORDER | self::SWP_NOACTIVATE,
        );
    }

    private function createBitmap(): void
    {
        $this->bitmapInfo = $this->gdi32->new('BITMAPINFO');
        $this->bitmapInfo->bmiHeader->biSize = self::BITMAPINFOHEADER_SIZE;
        $this->bitmapInfo->bmiHeader->biWidth = $this->imageWidth;
        $this->bitmapInfo->bmiHeader->biHeight = -$this->imageHeight;
        $this->bitmapInfo->bmiHeader->biPlanes = 1;
        $this->bitmapInfo->bmiHeader->biBitCount = 32;
        $this->bitmapInfo->bmiHeader->biCompression = 0;
    }

    private function loadPlaceholder(): void
    {
        $file = Layout::placeholderPath();
        $png = @file_get_contents($file);
        if ($png === false) {
            throw new RuntimeException('cannot read placeholder image: ' . $file);
        }

        $decoded = Png::decode($png);
        if ($decoded['width'] === $this->imageWidth && $decoded['height'] === $this->imageHeight) {
            FFI::memcpy($this->pixels, $decoded['bgra'], $this->fullBytes);
        } else {
            /*
            * Non-default window sizes clip the natural placeholder, as the Tk
            * canvas does, instead of failing to open the window
            */
            $width = min($decoded['width'], $this->imageWidth);
            $height = min($decoded['height'], $this->imageHeight);
            $sourcePitch = $decoded['width'] * 4;
            $targetPitch = $this->imageWidth * 4;
            for ($y = 0; $y < $height; $y++) {
                FFI::memcpy(
                    FFI::addr($this->pixels[$y * $targetPitch]),
                    substr($decoded['bgra'], $y * $sourcePitch, $width * 4),
                    $width * 4,
                );
            }
        }

        $this->placeholderVisible = true;
    }

    private function handleMessage(int $hwnd, int $msg, int $wparam, int $lparam): int
    {
        switch ($msg) {
            case self::WM_CLOSE:
                $this->queueQuit();
                $this->user32->DestroyWindow($hwnd);

                return 0;

            case self::WM_DESTROY:
                $this->hwnd = 0;
                $this->user32->PostQuitMessage(0);

                return 0;

            case self::WM_PAINT:
                $this->paint($hwnd);

                return 0;

            case self::WM_COMMAND:
                return $this->handleCommand($hwnd, $wparam);

            case self::WM_CTLCOLOREDIT:
                $invalid = $this->invalidField === null
                    ? 0
                    : ($this->widgets[self::FIELD_COMPONENTS[$this->invalidField]] ?? 0);
                if ($invalid !== 0 && $wparam === $invalid) {
                    $this->gdi32->SetBkColor($lparam, self::INVALID_COLOR);

                    return $this->invalidBrush;
                }
                break;

            case self::WM_CTLCOLORSTATIC:
                $this->gdi32->SetBkMode($lparam, self::TRANSPARENT);

                return $this->backgroundBrush;
        }

        return $this->user32->DefWindowProcW($hwnd, $msg, $wparam, $lparam);
    }

    private function handleCommand(int $hwnd, int $wparam): int
    {
        if (self::commandCode($wparam) !== self::BN_CLICKED) {
            return 0;
        }

        switch (self::commandId($wparam)) {
            case self::ID_ZOOM_OUT:
                $this->events[] = ['type' => 'zoomOut'];
                break;

            case self::ID_ZOOM_IN:
                $this->events[] = ['type' => 'zoomIn'];
                break;

            case self::ID_RESET:
                $this->events[] = ['type' => 'reset'];
                break;

            case self::ID_GO:
                $this->events[] = [
                    'type' => 'apply',
                    're' => $this->text->windowText($this->widgets[Layout::COMP_REAL]),
                    'im' => $this->text->windowText($this->widgets[Layout::COMP_IMAG]),
                    'depth' => $this->text->windowText($this->widgets[Layout::COMP_DEPTH]),
                ];
                break;

            case self::ID_SAVE:
                $this->events[] = ['type' => 'save'];
                break;

            case self::ID_PAN_UP:
                $this->events[] = ['type' => 'panUp'];
                break;

            case self::ID_PAN_DOWN:
                $this->events[] = ['type' => 'panDown'];
                break;

            case self::ID_PAN_LEFT:
                $this->events[] = ['type' => 'panLeft'];
                break;

            case self::ID_PAN_RIGHT:
                $this->events[] = ['type' => 'panRight'];
                break;

            case self::ID_QUIT:
                $this->queueQuit();
                $this->user32->DestroyWindow($hwnd);
                break;
        }

        return 0;
    }

    private function paint(int $hwnd): void
    {
        $paint = $this->paintStruct;
        $hdc = $this->user32->BeginPaint($hwnd, FFI::addr($paint));
        if ($hdc === 0) {
            return;
        }

        if ($this->pixels !== null && $this->bitmapInfo !== null) {
            if ($this->placeholderVisible) {
                $offset = 0;
                $sourceWidth = $this->imageWidth;
                $sourceHeight = $this->imageHeight;
            } elseif ($this->preview) {
                $offset = $this->previewOffset;
                $sourceWidth = $this->previewWidth;
                $sourceHeight = $this->previewHeight;
            } else {
                $offset = 0;
                $sourceWidth = $this->imageWidth;
                $sourceHeight = $this->imageHeight;
            }

            $this->bitmapInfo->bmiHeader->biWidth = $sourceWidth;
            $this->bitmapInfo->bmiHeader->biHeight = -$sourceHeight;
            $this->bitmapInfo->bmiHeader->biSizeImage = $sourceWidth * $sourceHeight * 4;

            $this->gdi32->SetStretchBltMode($hdc, self::COLORONCOLOR);
            $this->gdi32->StretchDIBits(
                $hdc,
                $this->imageX,
                $this->imageY,
                $this->imageWidth,
                $this->imageHeight,
                0,
                0,
                $sourceWidth,
                $sourceHeight,
                $this->gdi32->cast('void *', FFI::addr($this->pixels[$offset])),
                FFI::addr($this->bitmapInfo),
                self::DIB_RGB_COLORS,
                self::SRCCOPY,
            );
        }

        $this->user32->EndPaint($hwnd, FFI::addr($paint));
    }

    private function invalidateImage(): void
    {
        if ($this->hwnd === 0) {
            return;
        }

        $rect = $this->invalidateRect;
        $rect->left = $this->imageX;
        $rect->top = $this->imageY;
        $rect->right = $this->imageX + $this->imageWidth;
        $rect->bottom = $this->imageY + $this->imageHeight;
        $this->user32->InvalidateRect($this->hwnd, FFI::addr($rect), 0);
    }

    /**
     * Invalidate only the destination rows of one band.
     *
     * A preview band maps to rows scaled by PREVIEW_ZOOM. Bands for the hidden
     * layer are skipped; setImageMode() invalidates when the layer is shown.
     */
    private function invalidateRows(int $rowBegin, int $rowEnd, bool $preview): void
    {
        if ($this->hwnd === 0 || $preview !== $this->preview) {
            return;
        }

        if ($preview) {
            $top = $this->imageY + $rowBegin * self::PREVIEW_ZOOM;
            $bottom = $this->imageY + $rowEnd * self::PREVIEW_ZOOM;
        } else {
            $top = $this->imageY + $rowBegin;
            $bottom = $this->imageY + $rowEnd;
        }

        $top = max($top, $this->imageY);
        $bottom = min($bottom, $this->imageY + $this->imageHeight);
        if ($top >= $bottom) {
            return;
        }

        $rect = $this->invalidateRect;
        $rect->left = $this->imageX;
        $rect->top = $top;
        $rect->right = $this->imageX + $this->imageWidth;
        $rect->bottom = $bottom;
        $this->user32->InvalidateRect($this->hwnd, FFI::addr($rect), 0);
    }

    private function invalidateWidget(int $widget): void
    {
        if ($widget !== 0) {
            $this->user32->InvalidateRect($widget, null, 1);
        }
    }

    private function queueQuit(): void
    {
        if ($this->quitQueued) {
            return;
        }
        $this->quitQueued = true;
        $this->events[] = ['type' => 'quit'];
    }

    private function throwFailure(): void
    {
        $error = $this->failure;
        if ($error === null) {
            return;
        }
        $this->failure = null;
        throw new RuntimeException('Win32 callback failed: ' . $error->getMessage(), 0, $error);
    }

    private function lastError(): int
    {
        return $this->kernel32?->GetLastError() ?? 0;
    }

    private function assertOpen(): void
    {
        if ($this->user32 === null) {
            throw new LogicException('Win32 backend is not open');
        }
    }

    private function shutdown(): void
    {
        if ($this->user32 !== null) {
            if ($this->hwnd !== 0) {
                $this->user32->DestroyWindow($this->hwnd);
                $this->hwnd = 0;
            }

            /*
            * Drop the WM_QUIT posted by WM_DESTROY so a later open() in the
            * same process does not see a stale quit message.
            */
            $message = $this->user32->new('MSG');
            while ($this->user32->PeekMessageW(
                FFI::addr($message),
                0,
                0,
                0,
                self::PM_REMOVE,
            ) !== 0) {
            }
        }
        $this->hwnd = 0;

        if ($this->classRegistered && $this->user32 !== null && $this->className !== null) {
            $this->user32->UnregisterClassW(
                $this->user32->cast('uint16_t *', $this->className),
                $this->hInstance,
            );
        }
        $this->classRegistered = false;

        if ($this->invalidBrush !== 0 && $this->gdi32 !== null) {
            $this->gdi32->DeleteObject($this->invalidBrush);
        }
        $this->invalidBrush = 0;
        $this->font = 0;
        $this->backgroundBrush = 0;

        if ($this->timerPeriodActive && $this->winmm !== null) {
            try {
                $this->winmm->timeEndPeriod(self::TIMER_PERIOD_MS);
            } catch (Throwable) {
            }
        }
        $this->timerPeriodActive = false;
        $this->winmm = null;

        $this->className = null;
        $this->title = null;
        $this->wndProcHolder = null;
        $this->pixels = null;
        $this->bitmapInfo = null;
        $this->text = null;
        $this->dialog = null;
        $this->message = null;
        $this->paintStruct = null;
        $this->invalidateRect = null;

        $this->imageX = 0;
        $this->imageY = 0;
        $this->imageWidth = 0;
        $this->imageHeight = 0;
        $this->previewWidth = 0;
        $this->previewHeight = 0;
        $this->previewOffset = 0;
        $this->fullBytes = 0;
        $this->previewBytes = 0;
        $this->preview = false;
        $this->placeholderVisible = false;
        $this->busy = false;
        $this->invalidField = null;
        $this->widgets = [];
        $this->busyWidgets = [];
        $this->statusWidget = 0;
        $this->events = [];
        $this->quitQueued = false;
        $this->failure = null;
        $this->trace = false;
        $this->spin = false;
        $this->dispatchedMessages = 0;

        $this->user32 = null;
        $this->gdi32 = null;
        $this->kernel32 = null;
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
}
