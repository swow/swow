# Mandelbrot example

A Mandelbrot renderer that exercises the swow thread layer: worker threads, cross-thread queues, shared memory, and a native window loaded through FFI.

The numeric core is pure PHP. It iterates with the perturbation method: one reference orbit is computed with bcmath at the precision the zoom depth needs, and every pixel is iterated in double precision as a small offset from that reference. Preview frames use the plain double iteration below depth 1e10; full frames always use perturbation. No pixel work happens in FFI.

## GUI threads and CPU-bound work

This example is also a reference for one specific problem: a CPU-bound program with a native graphical interface. The render takes seconds at deep zoom while the window has to stay responsive, and every desktop toolkit has its own rules about which thread may touch the window.

Facilities used:

| Facility | Role here |
| --- | --- |
| `Swow\Thread` | render workers and one service thread (orbit build, PNG encode) |
| `Swow\SpscQueue` | per worker job queue and per worker completion token queue |
| `Swow\SharedBuffer` | full and preview framebuffers, reference orbit, glitch table |
| `Swow\Futex` | generation counter for cancellation |
| FFI | GUI backends only: Tcl/Tk, AppKit, user32/gdi32 |
| bcmath | reference orbit and slow-path pixels, at the depth's precision |

The render path and the UI loop use no coroutines: the queues and the futex block the calling thread directly. Coroutines are not needed because every thread either computes continuously or polls without waiting.

The model here is:

1. The UI thread only does UI work: pump events, copy finished pixel bands, update widgets. It never computes, never compresses, never waits on a result.
2. Rendering runs on worker threads. The bcmath reference orbit and the PNG encoder run on a separate service thread. The UI thread starts them and polls for a completion token in its normal tick.
3. Nothing on the UI thread blocks: result queues are polled with timeout 0, and the idle wait goes through the backend (Win32 `MsgWaitForMultipleObjectsEx` with 1 ms timer resolution, AppKit run loop, Tk `DoOneEvent` with `TCL_DONT_WAIT`).
4. Bulk data lives in shared memory: framebuffers, reference orbit, glitch coordinates. Small `SpscQueue` tokens carry ownership and ordering, because queue push/pop is a futex release/acquire pair and plain shared memory has no fences. A worker writes pixels first and pushes the token last.
5. Cancellation uses a generation counter in a `Futex`. A new view bumps it, stale jobs are dropped by the workers, and stale tokens are ignored by the UI thread, so a new frame never waits for an old one.
6. Every toolkit keeps its own constraint: Win32 keeps the message loop on the main thread and raises the timer resolution to 1 ms; AppKit is only touched from the main thread; Tk is driven from the main thread with a non-blocking event pump.
7. While a frame renders the inputs and action buttons are disabled and Quit stays enabled. Quit closes the window before joining the workers, so the window disappears at once.

Where to read the code: `src/App.php` for construction and startup, `src/App/WindowLoop.php` and `src/App/HeadlessRunner.php` for the two loops, `src/App/FramePipeline.php` for dispatch and result handling, `src/App/ServiceClient.php` for the build/save service, `src/Thread/Worker.php` and `src/Thread/ServiceThread.php` for the two thread bodies, `src/Buffer.php` for the shared layout, and `src/Gui/{Tk,Cocoa,Win32}.php` for the three event loops.

## Requirements

- PHP built with ZTS and the swow extension loaded.
- bcmath and zlib (both bundled with the builds used here).
- GUI: FFI enabled, plus the platform window library: Tk 8.6 on Linux (`libtcl8.6`, `libtk8.6`) with an X display, AppKit on macOS, user32/gdi32 on Windows.
- Tests that open a window need the platform session: `Xvfb` is enough on Linux; macOS needs a logged-in console session; Windows needs a desktop.
- Optional: `ffmpeg` with `x11grab` if you want to capture the window.

## Quick start

Headless PNG, no FFI needed:

```sh
php -d extension=swow bin/mandelbrot --headless --threads=8 --depth=1e10 --output=out.png
```

Window, needs a display:

```sh
php -d extension=swow bin/mandelbrot --threads=4 --depth=1e4
```

Add the JIT flags below for usable speed. Without them the app prints a note and runs much slower.

## Enable JIT

JIT belongs to opcache. On the builds used here opcache is compiled in, so no `zend_extension` line is needed:

```sh
php -d extension=swow \
    -d opcache.enable_cli=1 \
    -d opcache.jit=tracing \
    -d opcache.jit_buffer_size=64M \
    bin/mandelbrot --headless --depth=1e10 --output=out.png
```

- `opcache.enable_cli=1` is required: the CLI disables opcache by default.
- `opcache.jit_buffer_size=64M` is required: its default is 0, which disables JIT even when `opcache.jit` is set.

Check that it is on:

```sh
php -d opcache.enable_cli=1 -d opcache.jit=tracing -d opcache.jit_buffer_size=64M \
    -r 'print_r(opcache_get_status(false)["jit"]);'
```

The renderer cannot enable JIT from inside the process. When JIT is off it prints one line to stderr with the three flags above.

## FFI

FFI is used only by the GUI backends. The render path, the workers, and the PNG writer never call FFI.

- CLI SAPI allows FFI by default even with `ffi.enable=preload`.
- If a host restricts FFI, add `-d ffi.enable=1`.
- Headless rendering works with FFI disabled: `-d ffi.enable=0`.
- Worker threads never touch FFI, so GUI constraints do not leak into them.

If the GUI backend cannot start (no display, missing library), the app prints the reason and falls back to headless, writing the PNG.

## Platforms

| Platform | Backend |
| --- | --- |
| Linux | Tk through FFI (`libtcl8.6`, `libtk8.6`) |
| macOS | AppKit through FFI (`libobjc`, Cocoa, CoreGraphics) |
| Windows | Win32 through FFI (`user32`, `gdi32`, `comdlg32`) |

`src/Gui/Factory.php` selects the backend for the current platform.

## Options

```text
--headless        render without a window and write PNG only
--threads=N       worker thread count (default: CPU count - 1)
--width=N         image width in pixels (default: 1280)
--height=N        image height in pixels (default: 720)
--center=RE,IM    view center as decimal numbers
--depth=N         zoom depth relative to the default view (default: 1e4)
--iterations=N    base iteration limit (default: 1024)
--output=PATH     output PNG path (default: mandelbrot.png)
--frames=N        number of frames to render (default: 1)
--auto[=MS]       window only: zoom in once per completed frame, at least MS
                  apart (default 200); inputs and action buttons stay disabled
--verify          compare thread counts and sample the slow path
--help            show this help
```

The default center is a published deep-zoom seahorse valley coordinate:

```text
-0.743643887018689113498922249712800000007037228233600378464531
 0.131825904211894657975878336475200000004669779776843260798656
```

The fields carry it as decimal text, so a zoom from 1e4 to 1e30 keeps the same point. It sits just outside the set: at 1e30 the reference orbit escapes at iteration 6377, and the pixels that outlive it finish with direct double iteration instead of being reported as glitches. At the default depth 1e4 the view is the familiar seahorse valley.

## Window

- Window size is set at startup from `--width` and `--height` (default 1280x720); the toolbar takes 44 pixels and the rest is the image area. The imaginary axis points up in the picture.
- Fields: Real, Imag, Depth. Apply with Enter or the Go button. The fields are decimal text with unlimited precision: what is stored is what is shown, and a value survives a round trip through the fields unchanged.
- Buttons: zoom out, zoom in (factor 2), reset, Go, Save, Quit, and a four button pan pad (up, down, left, right). One pan press moves the centre by one eighth of the visible area in the arrow direction; the centre is updated as decimal text, so deep zooms keep every digit.
- A placeholder image is shown until the first pixels arrive; then a quarter resolution thumbnail appears and the full frame follows.
- While a frame is computing the fields and the nine buttons are disabled; Quit stays available and the window keeps processing events.
- Save opens the platform save dialog. Window mode never writes `--output` on its own.
- `--auto[=MS]` turns the window into a zoom loop: the title gains `(auto mode)`, the fields and the nine action buttons stay disabled, Quit stays enabled, and the view zooms in by two once per completed full frame, at least MS milliseconds apart. Every frame is rendered at full resolution; the cadence is the frame time, with MS as a lower bound. Headless runs reject `--auto`.

## Tests

Headless, no display needed:

```sh
php -d extension=swow tests/headless/buffer.php
php -d extension=swow tests/headless/config.php
php -d extension=swow tests/headless/image.php
php -d extension=swow tests/headless/render.php
php -d extension=swow tests/headless/consistency.php
php -d extension=swow tests/headless/window-flow.php
php -d extension=swow tests/headless/deep.php
```

`deep.php` renders the deep coordinate at 1e20 and 1e30 and checks that the images are not flat.

Numeric core, plain PHP, no swow and no display:

```sh
php tests/numeric/run.php          # fixtures and sampled bcmath agreement
php tests/numeric/bench.php        # per frame timings and glitch counts
php tests/numeric/profile.php      # cost breakdown of the render loops
php tests/numeric/render_png.php   # write reference PNGs for two depths
```

GUI, needs a display. Start one with `Xvfb :99 -screen 0 1400x900x24 &` and set `DISPLAY=:99`:

```sh
php tests/gui/layout.php       # layout tree only, no display needed
DISPLAY=:99 php tests/gui/tk-smoke.php
DISPLAY=:99 php tests/gui/tk-events.php
```

`tests/gui/drive.php` drives a running app through Tk `send`: start the app first, then run the driver. It checks zoom, reset, invalid input marking, apply and a clean quit.

On macOS, in a logged-in console session:

```sh
php tests/gui/cocoa-smoke.php
php tests/gui/cocoa-events.php
```

On Windows, from the example directory:

```bat
php -d ffi.enable=1 tests\gui\win32-smoke.php
```

`tests/gui/timing.php` starts its own Xvfb and app instances, runs three full sessions of ten zooms plus reset and apply, and writes `tests/gui/out/timing-table.md`.

A full gate that runs everything is:

```sh
php tests/run.php
```

It runs the nine headless tests, the numeric suite, and, when `DISPLAY` is set, the Tk layout/smoke/events tests, stopping at the first failure. Each script can still be run on its own.

## Layout

```text
bin/mandelbrot      entry point, options, mode selection
src/Config.php      parsed options
src/View.php        center strings, depth, scale, iteration limit
src/Reference.php   bcmath reference orbit
src/Renderer.php    direct and perturbation band loops
src/Buffer.php      shared framebuffer, orbit and glitch layout
src/App.php         construction, startup, mode dispatch
src/App/            window loop, headless runner, frame pipeline,
                    service client, trace
src/Thread/         worker thread and service thread bodies
src/Image.php       PNG writer
src/Gui/            layout tree, factory, Backend interface, three backends,
                    plus Cocoa/ObjC and Win32/{Png,Text,Dialog}
tests/              headless, numeric and GUI tests
tests/run.php       one command that runs every suite
tests/support.php   shared check/finish helpers
assets/             placeholder image
```

The shared buffer holds a 64-byte header, a full and a preview framebuffer, three double arrays for the reference orbit, and a glitch table. Workers take jobs from a per-worker `SpscQueue`, write whole row bands with one `SharedBuffer::writeU32` call, record glitch coordinates in the same buffer, then push a small completion token. The token queue is a plain futex pair, not a coroutine primitive, so the render path never enters the coroutine scheduler. The main thread reads finished tokens and hands rows to the backend (Tk with `Tk_PhotoPutBlock`, Cocoa with a CGImage, Windows with `StretchDIBits`).

## Known limits

- The window size is chosen at startup (`--width`, `--height`); resizing at runtime is not supported.
- No input method support; coordinates are typed as ASCII decimals.
- On Windows a single-line edit does not report Enter; use the Go button.
- Full deep frames take seconds; interaction relies on the quarter resolution preview.
- The frame timeout is hard coded to 600 seconds.

## License

Copyright (C) 2026 Yun Dou <dixyes@gmail.com>

You may use, modify and redistribute this demo without attribution.

```text
            DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
                    Version 2, December 2004

 Copyright (C) 2026 Yun Dou <dixyes@gmail.com>

 Everyone is permitted to copy and distribute verbatim or modified
 copies of this license document, and changing it is allowed as long
 as the name is changed.

            DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
   TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION

  0. You just DO WHAT THE FUCK YOU WANT TO.
```
