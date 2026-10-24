<?php

declare(strict_types=1);

/*
 * App window-loop flow with a fake backend.
 *
 * Checks the preview-first startup, the busy handshake and mid-frame quit
 * without a display. The real Tk window is verified separately.
 */

use Swow\Mandelbrot\App;
use Swow\Mandelbrot\Config;
use Swow\Mandelbrot\Gui\Backend;

require __DIR__ . '/../../src/bootstrap.php';
require __DIR__ . '/../support.php';

final class FakeBackend implements Backend
{
    /** @var list<bool> */
    public array $busy = [];

    /** @var list<array{preview: bool, rowBegin: int, rowEnd: int, width: int}> */
    public array $presented = [];

    /** @var list<string> */
    public array $statuses = [];

    /** @var list<string> */
    public array $titles = [];

    public bool $sawFull = false;

    private array $events = [];

    private float $started = 0.0;

    public function __construct(
        private readonly int $width,
        private readonly int $height,
        public ?float $quitAfterSeconds = null,
    ) {
    }

    public function open(int $windowWidth, int $windowHeight): void
    {
        $this->started = microtime(true);
    }

    public function imageWidth(): int
    {
        return $this->width;
    }

    public function imageHeight(): int
    {
        return $this->height;
    }

    public function pump(): array
    {
        if (
            $this->quitAfterSeconds !== null &&
            microtime(true) - $this->started >= $this->quitAfterSeconds
        ) {
            return [['type' => 'quit']];
        }

        $events = $this->events;
        $this->events = [];

        return $events;
    }

    public function setImageMode(bool $preview): void
    {
    }

    public function setTitle(string $title): void
    {
        $this->titles[] = $title;
    }

    public function presentRows(string $bytes, int $rowBegin, int $rowEnd, int $width, bool $preview): void
    {
        $this->presented[] = [
            'preview' => $preview,
            'rowBegin' => $rowBegin,
            'rowEnd' => $rowEnd,
            'width' => $width,
        ];
        if (!$preview) {
            $this->sawFull = true;
        }
    }

    public function setFields(string $re, string $im, string $depth): void
    {
    }

    public function markInvalid(?string $field, string $message): void
    {
    }

    public function status(string $text): void
    {
        $this->statuses[] = $text;
    }

    public function askSavePath(string $default): ?string
    {
        return null;
    }

    public function setBusy(bool $busy): void
    {
        $this->busy[] = $busy;
        if (!$busy && $this->sawFull) {
            $this->events[] = ['type' => 'quit'];
        }
    }

    public function wait(int $microseconds): void
    {
        // Exercise App's defensive fallback path.
        throw new RuntimeException('wait is not available');
    }

    public function close(): void
    {
    }
}

$common = [
    'mandelbrot',
    '--threads=2',
    '--width=160',
    '--height=90',
    '--output=' . tempPath('window-flow', '.png'),
];

// 1. Startup renders a preview first, then the idle full frame.
$gui = new FakeBackend(160, 90);
$config = Config::fromArgv([...$common, '--depth=1e4']);
check((new App($config, $gui))->run() === 0, 'preview-first run exits zero');
check($gui->busy !== [] && $gui->busy[0] === true, 'controls are busy at frame start');
check($gui->presented !== [] && $gui->presented[0]['preview'] === true, 'first presented frame is the preview');
check($gui->presented[count($gui->presented) - 1]['preview'] === false, 'a full frame follows the preview');
check($gui->busy[count($gui->busy) - 1] === false, 'controls are released after the full frame');
check(in_array('rendering preview', $gui->statuses, true), 'status announces the preview');
check(in_array('rendering', $gui->statuses, true), 'status announces the full refine');

// 2. Quit during a long frame exits zero and releases the controls.
$gui = new FakeBackend(320, 180, 0.05);
$config = Config::fromArgv([
    'mandelbrot',
    '--threads=4',
    '--width=320',
    '--height=180',
    '--depth=1e30',
    '--output=' . tempPath('window-flow-quit', '.png'),
]);
$started = microtime(true);
$exit = (new App($config, $gui))->run();
$elapsed = microtime(true) - $started;
check($exit === 0, 'mid-frame quit exits zero');
check($elapsed < 5.0, 'mid-frame quit joins the workers promptly');
check(!$gui->sawFull, 'quit happened before the full frame completed');
check($gui->busy !== [] && $gui->busy[0] === true, 'controls were busy during the long frame');
check($gui->busy[count($gui->busy) - 1] === false, 'controls are released on quit');

finish('window-flow');
