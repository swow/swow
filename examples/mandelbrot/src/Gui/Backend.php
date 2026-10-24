<?php

declare(strict_types=1);

namespace Swow\Mandelbrot\Gui;

/**
 * GUI backend contract.
 *
 * The frame loop calls open() once, pump() every tick and presentRows() for
 * the completed row bands of a frame. Preview and full frames share one sink;
 * setImageMode() picks which of them is visible.
 */
interface Backend
{
    // Lifecycle

    /**
     * Open the window.
     *
     * $width and $height are the window client size. The image area below the
     * toolbar is smaller; ask imageWidth() and imageHeight() for it.
     */
    public function open(int $width, int $height): void;

    /** Width of the image area, in pixels. */
    public function imageWidth(): int;

    /** Height of the image area, in pixels. */
    public function imageHeight(): int;

    /** Destroy the window and release the backend resources. */
    public function close(): void;

    // Events

    /**
     * Process pending window events without blocking.
     *
     * Returns queued events: zoomIn, zoomOut, reset, apply, save, quit,
     * panUp, panDown, panLeft, panRight. apply carries re, im and depth.
     *
     * @return list<array<string, mixed>>
     */
    public function pump(): array;

    // Image sink

    /**
     * Choose the image shown in the image area.
     *
     * Both images stay valid, only the visible one changes.
     */
    public function setImageMode(bool $preview): void;

    /**
     * Submit rows [$rowBegin, $rowEnd) of a frame.
     *
     * $bytes holds tightly packed BGRA pixels with alpha 255. $width is the
     * width of the submitted image: the frame width when $preview is false,
     * otherwise the 1/4 linear preview width.
     */
    public function presentRows(string $bytes, int $rowBegin, int $rowEnd, int $width, bool $preview): void;

    // Widgets and dialog

    /**
     * Disable or enable the input widgets while a frame is rendering.
     *
     * Quit and the status line stay usable. Calling with the current state is
     * a no-op.
     */
    public function setBusy(bool $busy): void;

    /** Set the window title shown by the platform window manager. */
    public function setTitle(string $title): void;

    /** Fill the three entry widgets. */
    public function setFields(string $re, string $im, string $depth): void;

    /**
     * Mark one entry as invalid and write the reason to the status line.
     *
     * $field is one of re, im, depth. Null clears all marks.
     */
    public function markInvalid(?string $field, string $message): void;

    /** Replace the status line. */
    public function status(string $text): void;

    /** Ask the user for a save path, null when cancelled. */
    public function askSavePath(string $default): ?string;

    // Wait

    /**
     * Sleep the calling thread for at least $microseconds.
     *
     * This uses the platform call directly instead of usleep() through the
     * coroutine runtime, so the GUI thread is not handed to the scheduler.
     */
    public function wait(int $microseconds): void;
}
