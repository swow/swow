<?php

declare(strict_types=1);

namespace Swow\Mandelbrot\Gui;

use FFI;
use FFI\CData;
use InvalidArgumentException;
use LogicException;
use RuntimeException;
use Throwable;

use function sprintf;
use function strlen;

use const PHP_BINARY;

/**
 * Tk backend loaded through FFI.
 *
 * One interpreter serves one window. The frame loop drives events with
 * pump(), so no Tk main loop runs. Tcl commands registered by the layout
 * append events to a PHP queue that pump() drains. close() finalizes Tcl for
 * the process, so the backend cannot be reopened afterwards.
 */
final class Tk implements Backend
{
    private const TCL_LIBRARY = 'libtcl8.6.so';
    private const TK_LIBRARY = 'libtk8.6.so';

    private const TCL_OK = 0;
    private const TCL_ERROR = 1;
    private const TCL_DONT_WAIT = 1 << 1;
    private const TCL_VOLATILE = 1;

    /*
    * Tk 8.6.16 exports the interp-first photo API declared by tkDecls.h
    *
    * TK_PHOTO_COMPOSITE_SET is 1 in Tk 8.6; alpha is always 255 here, so the
    * composite rule only matters if a caller submits non-opaque pixels
    */
    private const TK_PHOTO_COMPOSITE_SET = 1;
    private const PREVIEW_ZOOM = 4;
    private const INVALID_BACKGROUND = '#ff6666';

    private const ENTRY_WIDGETS = [
        're' => Layout::ENTRY_RE,
        'im' => Layout::ENTRY_IM,
        'depth' => Layout::ENTRY_DEPTH,
    ];

    private const BUSY_WIDGETS = [
        Layout::ENTRY_RE,
        Layout::ENTRY_IM,
        Layout::ENTRY_DEPTH,
        Layout::BUTTON_ZOOM_OUT,
        Layout::BUTTON_ZOOM_IN,
        Layout::BUTTON_RESET,
        Layout::BUTTON_GO,
        Layout::BUTTON_SAVE,
        Layout::BUTTON_PAN_UP,
        Layout::BUTTON_PAN_DOWN,
        Layout::BUTTON_PAN_LEFT,
        Layout::BUTTON_PAN_RIGHT,
    ];

    private const WIDGETS = [
        Layout::COMP_REAL => Layout::ENTRY_RE,
        Layout::COMP_IMAG => Layout::ENTRY_IM,
        Layout::COMP_DEPTH => Layout::ENTRY_DEPTH,
        Layout::COMP_ZOOM_OUT => Layout::BUTTON_ZOOM_OUT,
        Layout::COMP_ZOOM_IN => Layout::BUTTON_ZOOM_IN,
        Layout::COMP_RESET => Layout::BUTTON_RESET,
        Layout::COMP_GO => Layout::BUTTON_GO,
        Layout::COMP_SAVE => Layout::BUTTON_SAVE,
        Layout::COMP_QUIT => Layout::BUTTON_QUIT,
        Layout::COMP_PAN_UP => Layout::BUTTON_PAN_UP,
        Layout::COMP_PAN_DOWN => Layout::BUTTON_PAN_DOWN,
        Layout::COMP_PAN_LEFT => Layout::BUTTON_PAN_LEFT,
        Layout::COMP_PAN_RIGHT => Layout::BUTTON_PAN_RIGHT,
    ];

    private const COMMANDS = [
        Layout::COMP_ZOOM_OUT => Layout::CMD_ZOOM_OUT,
        Layout::COMP_ZOOM_IN => Layout::CMD_ZOOM_IN,
        Layout::COMP_RESET => Layout::CMD_RESET,
        Layout::COMP_GO => Layout::CMD_APPLY,
        Layout::COMP_SAVE => Layout::CMD_SAVE,
        Layout::COMP_QUIT => Layout::CMD_QUIT,
        Layout::COMP_PAN_UP => Layout::CMD_PAN_UP,
        Layout::COMP_PAN_DOWN => Layout::CMD_PAN_DOWN,
        Layout::COMP_PAN_LEFT => Layout::CMD_PAN_LEFT,
        Layout::COMP_PAN_RIGHT => Layout::CMD_PAN_RIGHT,
    ];

    private const TCL_CDEF = <<<'C'
    typedef int Tcl_ObjCmdProc(void *clientData, void *interp, int objc, void **objv);
    typedef struct {
        Tcl_ObjCmdProc *proc;
    } MandelCommand;
    void Tcl_FindExecutable(const char *argv0);
    void *Tcl_CreateInterp(void);
    void Tcl_DeleteInterp(void *interp);
    int Tcl_Init(void *interp);
    int Tcl_Eval(void *interp, const char *script);
    const char *Tcl_GetStringResult(void *interp);
    void *Tcl_CreateObjCommand(void *interp, const char *cmdName,
        Tcl_ObjCmdProc *proc, void *clientData, void *deleteProc);
    void Tcl_SetResult(void *interp, char *result, int freeProc);
    int Tcl_DoOneEvent(int flags);
    void Tcl_Finalize(void);
    C;

    private const TK_CDEF = <<<'C'
    typedef struct Tk_PhotoImageBlock {
        unsigned char *pixelPtr;
        int width;
        int height;
        int pitch;
        int pixelSize;
        int offset[4];
    } Tk_PhotoImageBlock;
    int Tk_Init(void *interp);
    void *Tk_FindPhoto(void *interp, const char *imageName);
    int Tk_PhotoPutBlock(void *interp, void *handle, Tk_PhotoImageBlock *block,
        int x, int y, int width, int height, int compRule);
    int Tk_PhotoPutZoomedBlock(void *interp, void *handle,
        Tk_PhotoImageBlock *block, int x, int y, int width, int height,
        int zoomX, int zoomY, int subsampleX, int subsampleY, int compRule);
    C;

    private ?FFI $tcl = null;
    private ?FFI $tk = null;
    private ?CData $interp = null;
    private bool $tclLive = false;

    private ?CData $fullPhoto = null;
    private ?CData $previewPhoto = null;

    /** Single BGRA staging buffer for all submitted bands. */
    private ?CData $pixels = null;
    private ?CData $block = null;

    private int $imageWidth = 0;
    private int $imageHeight = 0;

    /** Default entry background colors, restored by markInvalid(null). */
    private array $entryBackgrounds = [];

    /** Callback holders, one per registered Tcl command. */
    private array $commandHolders = [];

    /** @var list<array<string, mixed>> */
    private array $events = [];

    private bool $placeholderVisible = false;
    private bool $busy = false;

    /** Lazy libc handle for the direct usleep() call. */
    private ?FFI $libc = null;
    private bool $libcTried = false;

    public function open(int $width, int $height): void
    {
        if ($this->tcl !== null || $this->interp !== null) {
            throw new LogicException('Tk backend is already open');
        }

        $tree = Layout::tree();
        $rects = Layout::resolve($tree, Layout::metrics(), $width, $height);
        $canvas = $rects[Layout::COMP_CANVAS];
        $imageWidth = $canvas->w;
        $imageHeight = $canvas->h;
        if ($imageWidth < self::PREVIEW_ZOOM || $imageHeight < self::PREVIEW_ZOOM) {
            throw new InvalidArgumentException(sprintf(
                'image size %dx%d is too small for a 1/%d preview',
                $imageWidth,
                $imageHeight,
                self::PREVIEW_ZOOM,
            ));
        }

        $this->tcl = FFI::cdef(self::TCL_CDEF, self::TCL_LIBRARY);
        $this->tk = FFI::cdef(self::TK_CDEF, self::TK_LIBRARY);

        try {
            $this->tcl->Tcl_FindExecutable(PHP_BINARY);

            $interp = $this->tcl->Tcl_CreateInterp();
            if ($interp === null) {
                throw new RuntimeException('Tcl_CreateInterp failed');
            }
            $this->interp = $interp;

            if ($this->tcl->Tcl_Init($interp) !== self::TCL_OK) {
                throw new RuntimeException('Tcl_Init failed: ' . $this->result($interp));
            }
            $this->tclLive = true;

            if ($this->tk->Tk_Init($interp) !== self::TCL_OK) {
                throw new RuntimeException('Tk_Init failed: ' . $this->result($interp));
            }

            $this->installCommands();
            $this->evalScript($this->layoutScript($width, $height, $rects, $tree));
            $this->placeholderVisible = true;
            $this->busy = false;

            $this->imageWidth = $imageWidth;
            $this->imageHeight = $imageHeight;
            $this->entryBackgrounds = [
                're' => $this->evalScript(Layout::ENTRY_RE . ' cget -background'),
                'im' => $this->evalScript(Layout::ENTRY_IM . ' cget -background'),
                'depth' => $this->evalScript(Layout::ENTRY_DEPTH . ' cget -background'),
            ];

            $full = $this->tk->Tk_FindPhoto($interp, Layout::FULL_IMAGE);
            $preview = $this->tk->Tk_FindPhoto($interp, Layout::PREVIEW_IMAGE);
            if ($full === null || $preview === null) {
                throw new RuntimeException('Tk_FindPhoto failed for the layout photo images');
            }
            $this->fullPhoto = $full;
            $this->previewPhoto = $preview;

            $this->pixels = $this->tcl->new('unsigned char[' . ($imageWidth * $imageHeight * 4) . ']');

            $block = $this->tk->new('Tk_PhotoImageBlock');
            $block->pitch = 0;
            $block->pixelSize = 4;
            $block->offset[0] = 2;
            $block->offset[1] = 1;
            $block->offset[2] = 0;
            $block->offset[3] = 3;
            $this->block = $block;
        } catch (Throwable $e) {
            $this->shutdown();
            throw $e;
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

        while ($this->tcl->Tcl_DoOneEvent(self::TCL_DONT_WAIT) === 1) {
        }

        $events = $this->events;
        $this->events = [];

        return $events;
    }

    public function setImageMode(bool $preview): void
    {
        $this->assertOpen();

        $this->evalScript(Layout::CANVAS . ($preview ? ' raise preview' : ' raise full'));
    }

    public function setBusy(bool $busy): void
    {
        $this->assertOpen();
        if ($busy === $this->busy) {
            return;
        }
        $this->busy = $busy;

        $state = $busy ? 'disabled' : 'normal';
        $commands = [];
        foreach (self::BUSY_WIDGETS as $widget) {
            $commands[] = $widget . ' configure -state ' . $state;
        }
        $this->evalScript(implode('; ', $commands));
    }

    public function setTitle(string $title): void
    {
        $this->assertOpen();
        $this->evalScript('wm title . ' . self::quote($title));
    }

    public function wait(int $microseconds): void
    {
        $microseconds = max(1, $microseconds);
        if (!$this->libcTried) {
            $this->libcTried = true;
            try {
                $this->libc = FFI::cdef('int usleep(unsigned int usec);', 'libc.so.6');
            } catch (Throwable) {
                $this->libc = null;
            }
        }

        if ($this->libc !== null) {
            try {
                if ($this->libc->usleep($microseconds) === 0) {
                    return;
                }
            } catch (Throwable) {
            }
        }

        usleep($microseconds);
    }

    public function presentRows(string $bytes, int $rowBegin, int $rowEnd, int $width, bool $preview): void
    {
        $this->assertOpen();

        if ($this->placeholderVisible) {
            $this->evalScript(
                Layout::CANVAS . ' delete ' . Layout::PLACEHOLDER_TAG
                . '; image delete ' . Layout::PLACEHOLDER_IMAGE,
            );
            $this->placeholderVisible = false;
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

        if ($preview) {
            $expectedWidth = intdiv($this->imageWidth, self::PREVIEW_ZOOM);
            $expectedHeight = intdiv($this->imageHeight, self::PREVIEW_ZOOM);
        } else {
            $expectedWidth = $this->imageWidth;
            $expectedHeight = $this->imageHeight;
        }
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

        $offset = $rowBegin * $width * 4;
        FFI::memcpy(FFI::addr($this->pixels[$offset]), $bytes, $expectedBytes);

        $this->block->pixelPtr = $this->tk->cast('unsigned char *', FFI::addr($this->pixels[$offset]));
        $this->block->width = $width;
        $this->block->height = $rows;
        $this->block->pitch = $width * 4;

        if ($preview) {
            /*
            * Tk_PhotoPutZoomedBlock takes destination dimensions; the zoom
            * factors expand the source block to that area
            */
            $rc = $this->tk->Tk_PhotoPutZoomedBlock(
                $this->interp,
                $this->previewPhoto,
                FFI::addr($this->block),
                0,
                $rowBegin * self::PREVIEW_ZOOM,
                $width * self::PREVIEW_ZOOM,
                $rows * self::PREVIEW_ZOOM,
                self::PREVIEW_ZOOM,
                self::PREVIEW_ZOOM,
                1,
                1,
                self::TK_PHOTO_COMPOSITE_SET,
            );
            if ($rc !== self::TCL_OK) {
                throw new RuntimeException(
                    'Tk_PhotoPutZoomedBlock failed: ' . $this->result($this->interp),
                );
            }

            return;
        }

        $rc = $this->tk->Tk_PhotoPutBlock(
            $this->interp,
            $this->fullPhoto,
            FFI::addr($this->block),
            0,
            $rowBegin,
            $width,
            $rows,
            self::TK_PHOTO_COMPOSITE_SET,
        );
        if ($rc !== self::TCL_OK) {
            throw new RuntimeException(
                'Tk_PhotoPutBlock failed: ' . $this->result($this->interp),
            );
        }
    }

    public function setFields(string $re, string $im, string $depth): void
    {
        $this->assertOpen();

        $values = ['re' => $re, 'im' => $im, 'depth' => $depth];
        foreach ($values as $field => $value) {
            $widget = self::ENTRY_WIDGETS[$field];
            $state = $this->evalScript($widget . ' cget -state');
            if ($state !== 'normal') {
                $this->evalScript($widget . ' configure -state normal');
            }
            try {
                $this->evalScript(
                    $widget . ' delete 0 end; ' . $widget . ' insert 0 ' . self::quote($value),
                );
            } finally {
                if ($state !== 'normal') {
                    $this->evalScript($widget . ' configure -state ' . self::quote($state));
                }
            }
        }
    }

    public function markInvalid(?string $field, string $message): void
    {
        $this->assertOpen();

        if ($field === null) {
            foreach (self::ENTRY_WIDGETS as $name => $widget) {
                $this->evalScript(
                    $widget . ' configure -background ' . self::quote($this->entryBackgrounds[$name]),
                );
            }
        } else {
            if (!isset(self::ENTRY_WIDGETS[$field])) {
                throw new InvalidArgumentException('unknown field: ' . $field);
            }
            $this->evalScript(
                self::ENTRY_WIDGETS[$field] . ' configure -background '
                . self::quote(self::INVALID_BACKGROUND),
            );
        }

        $this->status($message);
    }

    public function status(string $text): void
    {
        $this->assertOpen();
        $this->evalScript(Layout::STATUS . ' configure -text ' . self::quote($text));
    }

    public function askSavePath(string $default): ?string
    {
        $this->assertOpen();

        $path = $this->evalScript(
            'tk_getSaveFile -title {Save Mandelbrot PNG}'
            . ' -defaultextension .png'
            . ' -filetypes {{PNG {.png}} {All files {*}}}'
            . ' -initialfile ' . self::quote($default),
        );

        return $path === '' ? null : $path;
    }

    public function close(): void
    {
        $this->shutdown();
    }

    /**
     * Run a Tcl command in the window interpreter.
     *
     * Used for photo export and by tests; the frame loop uses the typed
     * methods above.
     */
    public function evaluate(string $script): string
    {
        $this->assertOpen();

        return $this->evalScript($script);
    }

    /**
     * Build the Tcl layout script from the resolved rects.
     *
     * Tk on this host maps one DIP to one pixel; the backend places every
     * widget at the resolved geometry instead of relying on pack.
     *
     * @param array<string, Rect> $rects
     */
    private function layoutScript(int $windowWidth, int $windowHeight, array $rects, Node $tree): string
    {
        $canvas = $rects[Layout::COMP_CANVAS];
        $lines = [
            'wm title . ' . self::quote(Layout::TITLE),
            'wm resizable . 0 0',
            sprintf('wm geometry . %dx%d', $windowWidth, $windowHeight),
            'font create MandelSmallFont -size 8',
            'frame ' . Layout::TOOLBAR . ' -borderwidth 0',
            self::placeCommand(Layout::TOOLBAR, new Rect(0, 0, $windowWidth, $canvas->y)),
        ];

        foreach (self::leaves($tree) as $leaf) {
            if ($leaf->leaf === LeafKind::Spacer || $leaf->key === '') {
                continue;
            }
            $rect = $rects[$leaf->key] ?? throw new RuntimeException(
                'missing layout rect for ' . $leaf->key,
            );

            switch ($leaf->leaf) {
                case LeafKind::Label:
                    if ($leaf->component === Layout::COMP_STATUS) {
                        $path = Layout::STATUS;
                        $lines[] = 'label ' . $path . ' -anchor w -font MandelSmallFont'
                            . ' -borderwidth 0 -highlightthickness 0 -pady 0 -text {}';
                    } else {
                        $path = Layout::TOOLBAR . '.' . strtolower($leaf->text) . 'Label';
                        $lines[] = 'label ' . $path . ' -text ' . self::quote($leaf->text);
                    }
                    $lines[] = self::placeCommand($path, $rect);
                    break;

                case LeafKind::Edit:
                    $component = $leaf->component ?? throw new RuntimeException(
                        'edit leaf without a component',
                    );
                    $path = self::WIDGETS[$component];
                    $lines[] = 'entry ' . $path;
                    $lines[] = self::placeCommand($path, $rect);
                    $lines[] = 'bind ' . $path . ' <Return> {' . Layout::CMD_APPLY . '}';
                    break;

                case LeafKind::Button:
                    $component = $leaf->component ?? throw new RuntimeException(
                        'button leaf without a component',
                    );
                    $path = self::WIDGETS[$component];
                    $lines[] = 'button ' . $path . ' -text ' . self::quote($leaf->text)
                        . ' -pady 0 -command {' . self::COMMANDS[$component] . '}';
                    $lines[] = self::placeCommand($path, $rect);
                    break;
            }
        }

        $lines[] = sprintf(
            'canvas %s -width %d -height %d -borderwidth 0'
                . ' -highlightthickness 0 -background {#000000}',
            Layout::CANVAS,
            $canvas->w,
            $canvas->h,
        );
        $lines[] = self::placeCommand(Layout::CANVAS, $canvas);
        $lines[] = sprintf(
            'image create photo %s -width %d -height %d',
            Layout::FULL_IMAGE,
            $canvas->w,
            $canvas->h,
        );
        $lines[] = sprintf(
            'image create photo %s -width %d -height %d',
            Layout::PREVIEW_IMAGE,
            $canvas->w,
            $canvas->h,
        );
        $lines[] = '.canvas create image 0 0 -anchor nw -image ' . Layout::FULL_IMAGE . ' -tags {full}';
        $lines[] = '.canvas create image 0 0 -anchor nw -image ' . Layout::PREVIEW_IMAGE . ' -tags {preview}';
        $lines[] = '.canvas raise full';
        $lines[] = 'image create photo ' . Layout::PLACEHOLDER_IMAGE
            . ' -file ' . self::quote(Layout::placeholderPath());
        $lines[] = '.canvas create image 0 0 -anchor nw -image ' . Layout::PLACEHOLDER_IMAGE
            . ' -tags {' . Layout::PLACEHOLDER_TAG . '}';
        $lines[] = '.canvas raise ' . Layout::PLACEHOLDER_TAG;

        return implode("\n", $lines);
    }

    private static function placeCommand(string $path, Rect $rect): string
    {
        return sprintf(
            'place %s -x %d -y %d -width %d -height %d',
            $path,
            $rect->x,
            $rect->y,
            $rect->w,
            $rect->h,
        );
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

    private function installCommands(): void
    {
        $this->addCommand(Layout::CMD_ZOOM_OUT, function (): void {
            $this->events[] = ['type' => 'zoomOut'];
        });
        $this->addCommand(Layout::CMD_ZOOM_IN, function (): void {
            $this->events[] = ['type' => 'zoomIn'];
        });
        $this->addCommand(Layout::CMD_RESET, function (): void {
            $this->events[] = ['type' => 'reset'];
        });
        $this->addCommand(Layout::CMD_APPLY, function (): void {
            $this->events[] = [
                'type' => 'apply',
                're' => $this->evalScript(Layout::ENTRY_RE . ' get'),
                'im' => $this->evalScript(Layout::ENTRY_IM . ' get'),
                'depth' => $this->evalScript(Layout::ENTRY_DEPTH . ' get'),
            ];
        });
        $this->addCommand(Layout::CMD_SAVE, function (): void {
            $this->events[] = ['type' => 'save'];
        });
        $this->addCommand(Layout::CMD_QUIT, function (): void {
            $this->events[] = ['type' => 'quit'];
        });
        $this->addCommand(Layout::CMD_PAN_UP, function (): void {
            $this->events[] = ['type' => 'panUp'];
        });
        $this->addCommand(Layout::CMD_PAN_DOWN, function (): void {
            $this->events[] = ['type' => 'panDown'];
        });
        $this->addCommand(Layout::CMD_PAN_LEFT, function (): void {
            $this->events[] = ['type' => 'panLeft'];
        });
        $this->addCommand(Layout::CMD_PAN_RIGHT, function (): void {
            $this->events[] = ['type' => 'panRight'];
        });
    }

    /**
     * Register one Tcl command backed by a PHP closure.
     *
     * Assigning a closure to a function pointer field of the holder creates a
     * persistent C callback; a plain PHP variable assignment would not convert.
     * Tcl keeps the C function pointer itself, so the holder CData must stay
     * alive in commandHolders until the interpreter is deleted.
     */
    private function addCommand(string $name, callable $handler): void
    {
        $holder = $this->tcl->new('MandelCommand');
        $holder->proc = function ($clientData, $interp, int $objc, $objv) use ($handler): int {
            try {
                $handler();

                return self::TCL_OK;
            } catch (Throwable $error) {
                $this->tcl->Tcl_SetResult($interp, $error->getMessage(), self::TCL_VOLATILE);

                return self::TCL_ERROR;
            }
        };

        $command = $this->tcl->Tcl_CreateObjCommand($this->interp, $name, $holder->proc, null, null);
        if ($command === null) {
            throw new RuntimeException('Tcl_CreateObjCommand failed: ' . $name);
        }
        $this->commandHolders[$name] = $holder;
    }

    private function evalScript(string $script): string
    {
        $interp = $this->interp;
        if ($interp === null || $this->tcl === null) {
            throw new LogicException('Tk backend is not open');
        }

        if ($this->tcl->Tcl_Eval($interp, $script) !== self::TCL_OK) {
            throw new RuntimeException('Tcl error: ' . $this->result($interp));
        }

        return $this->result($interp);
    }

    private function result(CData $interp): string
    {
        return (string) $this->tcl->Tcl_GetStringResult($interp);
    }

    private function assertOpen(): void
    {
        if ($this->interp === null || $this->tcl === null) {
            throw new LogicException('Tk backend is not open');
        }
    }

    private function shutdown(): void
    {
        if ($this->interp !== null && $this->tcl !== null) {
            $this->tcl->Tcl_DeleteInterp($this->interp);
        }
        $this->interp = null;

        if ($this->tclLive && $this->tcl !== null) {
            $this->tcl->Tcl_Finalize();
        }
        $this->tclLive = false;

        $this->tcl = null;
        $this->tk = null;
        $this->fullPhoto = null;
        $this->previewPhoto = null;
        $this->pixels = null;
        $this->block = null;
        $this->imageWidth = 0;
        $this->imageHeight = 0;
        $this->entryBackgrounds = [];
        $this->commandHolders = [];
        $this->events = [];
        $this->placeholderVisible = false;
        $this->busy = false;
    }

    /**
     * Quote a string as a Tcl double-quoted word.
     */
    private static function quote(string $value): string
    {
        return '"' . strtr($value, [
            '\\' => '\\\\',
            '"' => '\"',
            '$' => '\$',
            '[' => '\[',
            ']' => '\]',
            "\n" => '\n',
            "\r" => '\r',
        ]) . '"';
    }
}
