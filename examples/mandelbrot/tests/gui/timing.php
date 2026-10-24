#!/usr/bin/env php
<?php
declare(strict_types=1);

/*
* Wall-clock timing harness for the GUI frame pipeline.
*
* The script starts a dedicated Xvfb display, starts the real app three times
* (1280x720, 4 threads, JIT, depth 1e4, default centre), waits for the initial
* preview and its full refine, drives ten zoomIn steps, one reset and one valid
* apply at 1e5 per run, then quits the app with ::mandel::quit and verifies
* exit 0.
*
* Status is polled every 5 ms through Tk send for startup, transitions and the
* final ready. S1 logs every completed frame:
*   frame N kind=preview|full depth=<g> full=<s> wall=<s> ...
* For each interactive step:
*   preview_ms = wall= from the matching kind=preview line
*   full_ms    = wall= from the matching kind=full line
*   total_ms   = send -> final "ready", including the ~150 ms idle gap
* The status-derived preview time is kept in the detail log as a cross-check.
* Startup is t0 -> first "ready" (the initial preview frame); the initial full
* refine is awaited before the zoom steps but is not part of the startup row.
*
* Run from swow/examples/mandelbrot:
*   php tests/gui/timing.php
*/

$root = dirname(__DIR__, 2);
require_once $root . '/src/Gui/Backend.php';
require_once $root . '/src/Gui/Layout.php';
require_once $root . '/src/Gui/Tk.php';

use Swow\Mandelbrot\Gui\Layout;
use Swow\Mandelbrot\Gui\Tk;

const RUNS = 3;
const ZOOM_STEPS = 10;
const POLL_US = 5000;
const STEP_TIMEOUT_SECONDS = 30.0;
const OUT_DIR = __DIR__ . '/out';

function fail(string $message): never
{
    throw new RuntimeException($message);
}

function tclName(string $name): string
{
    return '{' . str_replace(['\\', '{', '}'], ['\\\\', '\\{', '\\}'], $name) . '}';
}

/** @param list<float> $values */
function median(array $values): float
{
    $count = count($values);
    if ($count === 0) {
        return 0.0;
    }
    sort($values);
    $middle = intdiv($count, 2);
    if ($count % 2 === 1) {
        return $values[$middle];
    }

    return ($values[$middle - 1] + $values[$middle]) / 2;
}

/**
 * @param list<float> $values
 */
function statsCell(array $values): string
{
    if ($values === []) {
        return 'n/a';
    }

    return sprintf('%.1f / %.1f / %.1f', min($values), median($values), max($values));
}

function displayUp(string $display): bool
{
    exec('xdpyinfo -display ' . escapeshellarg($display) . ' >/dev/null 2>&1', $output, $code);

    return $code === 0;
}

final class TimingRun
{
    private $process = null;

    /** @var array<int, resource> */
    private array $pipes = [];

    private string $stdoutBuffer = '';
    private string $stderrBuffer = '';

    /** @var list<array{ms: float, line: string}> */
    private array $stdoutLines = [];

    /** @var list<array{ms: float, line: string}> */
    private array $stderrLines = [];

    private string $target = '';
    private int $t0 = 0;

    /**
     * @param array<string, string> $env
     */
    public function __construct(
        private readonly Tk $monitor,
        private readonly array $env,
        private readonly string $root,
    ) {
    }

    public function start(): void
    {
        $command = [
            PHP_BINARY,
            '-d', 'extension=swow',
            '-d', 'opcache.enable_cli=1',
            '-d', 'opcache.jit=tracing',
            '-d', 'opcache.jit_buffer_size=64M',
            'bin/mandelbrot',
            '--width=1280',
            '--height=720',
            '--threads=4',
            '--depth=1e4',
            '--output=timing.png',
        ];
        $descriptors = [
            0 => ['file', '/dev/null', 'r'],
            1 => ['pipe', 'w'],
            2 => ['pipe', 'w'],
        ];

        $this->t0 = hrtime(true);
        $this->process = proc_open($command, $descriptors, $this->pipes, $this->root, $this->env);
        if (!is_resource($this->process)) {
            fail('cannot start the app');
        }
        stream_set_blocking($this->pipes[1], false);
        stream_set_blocking($this->pipes[2], false);
    }

    public function __destruct()
    {
        $this->finish();
    }

    public function discover(): void
    {
        $own = $this->monitor->evaluate('tk appname');
        $deadline = microtime(true) + 15.0;
        do {
            $this->drain();
            $count = (int) $this->monitor->evaluate('llength [winfo interps]');
            for ($i = 0; $i < $count; $i++) {
                $name = $this->monitor->evaluate('lindex [winfo interps] ' . $i);
                if ($name === $own) {
                    continue;
                }
                try {
                    $depth = $this->monitor->evaluate(
                        'send ' . tclName($name) . ' {' . Layout::TOOLBAR . '.depth get}',
                    );
                } catch (RuntimeException) {
                    continue;
                }
                if ($depth !== '') {
                    $this->target = $name;

                    return;
                }
            }
            usleep(20_000);
        } while (microtime(true) < $deadline);

        fail('no target Tk application appeared');
    }

    public function send(string $script): string
    {
        return $this->monitor->evaluate('send ' . tclName($this->target) . ' {' . $script . '}');
    }

    public function status(): string
    {
        return $this->send('.toolbar.status cget -text');
    }

    public function field(string $name): string
    {
        return $this->send(Layout::TOOLBAR . '.' . $name . ' get');
    }

    /**
     * @return array{ms: float, depth: string, transitions: list<array{ms: float, status: string}>}
     */
    public function measureStartup(): array
    {
        $transitions = [];
        $last = null;
        $busy = false;
        $ready = null;
        $deadline = microtime(true) + STEP_TIMEOUT_SECONDS;
        while (microtime(true) < $deadline) {
            $this->drain();
            $status = $this->status();
            $ms = (hrtime(true) - $this->t0) / 1e6;
            if ($status !== $last) {
                $transitions[] = ['ms' => $ms, 'status' => $status];
                $last = $status;
            }
            if (!str_starts_with($status, 'ready')) {
                $busy = true;
            } elseif ($busy || $ms > 1000.0) {
                $ready = $ms;
                break;
            }
            usleep(POLL_US);
        }
        if ($ready === null) {
            fail('startup never became ready; transitions ' . json_encode($transitions));
        }

        return [
            'ms' => $ready,
            'depth' => $this->field('depth'),
            'transitions' => $transitions,
        ];
    }

    /**
     * @return array{
     *     depth: string,
     *     preview_ms: ?float,
     *     full_ms: float,
     *     status_full_ms: ?float,
     *     stderr_full_ms: float,
     *     stderr_frame: int,
     *     total_ms: float,
     *     idle_ms: ?float,
     *     first_ready_ms: float,
     *     transitions: list<array{ms: float, status: string}>
     * }
     */
    public function measureStep(string $script): array
    {
        $this->drain();
        $stderrMark = count($this->stderrLines);
        $send = hrtime(true);
        $this->send($script);

        $transitions = [];
        $last = null;
        $phase = 0; // 0: wait preview start, 1: wait preview ready, 2: wait full start, 3: wait full ready
        $previewStart = null;
        $previewReady = null;
        $fullStart = null;
        $fullReady = null;
        $deadline = microtime(true) + STEP_TIMEOUT_SECONDS;

        while (microtime(true) < $deadline) {
            $this->drain();
            $status = $this->status();
            $now = hrtime(true);
            if ($status !== $last) {
                $transitions[] = ['ms' => ($now - $send) / 1e6, 'status' => $status];
                $last = $status;
            }

            switch ($phase) {
                case 0:
                    if ($status === 'rendering preview') {
                        $previewStart = $now;
                        $phase = 1;
                    } elseif ($status === 'rendering') {
                        $fullStart = $now;
                        $phase = 3;
                    }
                    break;

                case 1:
                    if (str_starts_with($status, 'ready')) {
                        $previewReady = $now;
                        $phase = 2;
                    } elseif ($status === 'rendering') {
                        $fullStart = $now;
                        $phase = 3;
                    }
                    break;

                case 2:
                    if ($status === 'rendering') {
                        $fullStart = $now;
                        $phase = 3;
                    }
                    break;

                case 3:
                    if (str_starts_with($status, 'ready')) {
                        $fullReady = $now;
                    }
                    break;
            }

            if ($fullReady !== null) {
                break;
            }
            usleep(POLL_US);
        }

        if ($fullReady === null) {
            fail('step timed out: ' . $script . '; transitions ' . json_encode($transitions));
        }

        for ($i = 0; $i < 5; $i++) {
            $this->drain();
            usleep(2000);
        }

        $depth = $this->field('depth');
        $expected = (float) $depth;
        $stats = $this->frameStatsAfter($stderrMark);
        $previewStat = null;
        $fullStat = null;
        foreach ($stats as $stat) {
            if (abs($stat['depth'] - $expected) > max(1e-9, abs($expected) * 1e-6)) {
                continue;
            }
            if ($stat['kind'] === 'preview') {
                $previewStat = $stat;
            } elseif ($stat['kind'] === 'full') {
                $fullStat = $stat;
            }
        }
        if ($fullStat === null) {
            fail(
                'no stderr full-frame stats for ' . $script . ' at depth ' . $depth
                . '; lines ' . json_encode(array_column($stats, 'line')),
            );
        }

        $statusPreviewMs = $previewStart !== null && $previewReady !== null
            ? ($previewReady - $previewStart) / 1e6
            : null;
        $statusFullMs = $fullStart !== null ? ($fullReady - $fullStart) / 1e6 : null;
        $idleMs = $previewReady !== null && $fullStart !== null
            ? ($fullStart - $previewReady) / 1e6
            : null;

        return [
            'depth' => $depth,
            'preview_ms' => $previewStat !== null
                ? $previewStat['wall_s'] * 1000.0
                : $statusPreviewMs,
            'status_preview_ms' => $statusPreviewMs,
            'full_ms' => $fullStat['wall_s'] * 1000.0,
            'status_full_ms' => $statusFullMs,
            'stderr_full_ms' => $fullStat['full_s'] * 1000.0,
            'stderr_frame' => $fullStat['frame'],
            'preview_frame' => $previewStat['frame'] ?? null,
            'total_ms' => ($fullReady - $send) / 1e6,
            'idle_ms' => $idleMs,
            'first_ready_ms' => $previewReady !== null
                ? ($previewReady - $send) / 1e6
                : ($fullReady - $send) / 1e6,
            'transitions' => $transitions,
        ];
    }

    /**
     * @return list<array{frame: int, kind: string, depth: float, full_s: float, wall_s: float, line: string}>
     */
    private function frameStatsAfter(int $mark): array
    {
        $stats = [];
        foreach (array_slice($this->stderrLines, $mark) as $entry) {
            if (preg_match(
                '/^frame (\d+) kind=(preview|full) depth=([0-9.eE+\-]+) full=([0-9.]+)s'
                    . ' wall=([0-9.]+)s glitches=\d+ bands=\d+ reference=[0-9.]+s$/',
                $entry['line'],
                $matches,
            ) !== 1) {
                continue;
            }
            $stats[] = [
                'frame' => (int) $matches[1],
                'kind' => $matches[2],
                'depth' => (float) $matches[3],
                'full_s' => (float) $matches[4],
                'wall_s' => (float) $matches[5],
                'line' => $entry['line'],
            ];
        }

        return $stats;
    }

    /**
     * Wait for the initial full refine that follows the startup preview.
     *
     * The app starts with a preview frame and starts the full refine after its
     * idle gap. Timing steps must start from a settled window, otherwise the
     * initial full frame's status transitions collide with the first zoom.
     */
    public function settleInitialFrame(float $timeout = 10.0): void
    {
        $deadline = microtime(true) + $timeout;
        $searchUntil = microtime(true) + 1.0;
        $sawRendering = false;
        while (microtime(true) < $deadline) {
            $this->drain();
            $status = $this->status();
            if ($status === 'rendering') {
                $sawRendering = true;
            }
            if ($sawRendering && str_starts_with($status, 'ready')) {
                return;
            }
            if (!$sawRendering && microtime(true) > $searchUntil) {
                return;
            }
            usleep(POLL_US);
        }
        fail('initial full frame did not settle');
    }

    /** @return int process exit code */
    public function quit(): int
    {
        try {
            $this->send('::mandel::quit');
        } catch (RuntimeException) {
        }

        $exit = null;
        $deadline = microtime(true) + 15.0;
        while (microtime(true) < $deadline) {
            $this->drain();
            $status = proc_get_status($this->process);
            if (!$status['running']) {
                $exit = (int) $status['exitcode'];
                break;
            }
            usleep(POLL_US);
        }
        if ($exit === null) {
            fail('app did not exit after ::mandel::quit');
        }

        for ($i = 0; $i < 50; $i++) {
            $this->drain();
            usleep(2000);
        }

        return $exit;
    }

    public function drain(): void
    {
        $this->consume($this->pipes[1] ?? null, $this->stdoutBuffer, $this->stdoutLines);
        $this->consume($this->pipes[2] ?? null, $this->stderrBuffer, $this->stderrLines);
    }

    /** @return list<array{ms: float, line: string}> */
    public function stderrLines(): array
    {
        return $this->stderrLines;
    }

    /** @return list<array{ms: float, line: string}> */
    public function stdoutLines(): array
    {
        return $this->stdoutLines;
    }

    /** @return list<array{ms: float, line: string}> */
    public function warnings(): array
    {
        return array_values(array_filter(
            $this->stderrLines,
            static fn (array $entry): bool => preg_match(
                '/warning|notice|deprecated|fatal|uncaught/i',
                $entry['line'],
            ) === 1,
        ));
    }

    public function finish(): void
    {
        if (!is_resource($this->process)) {
            return;
        }

        $status = proc_get_status($this->process);
        if ($status['running']) {
            proc_terminate($this->process, 15);
            $deadline = microtime(true) + 2.0;
            while (microtime(true) < $deadline) {
                usleep(20_000);
                $status = proc_get_status($this->process);
                if (!$status['running']) {
                    break;
                }
            }
            if ($status['running']) {
                proc_terminate($this->process, 9);
            }
        }

        foreach ($this->pipes as $pipe) {
            if (is_resource($pipe)) {
                fclose($pipe);
            }
        }
        $this->pipes = [];
        proc_close($this->process);
        $this->process = null;
    }

    private function consume($stream, string &$buffer, array &$lines): void
    {
        if (!is_resource($stream)) {
            return;
        }
        $chunk = @stream_get_contents($stream);
        if ($chunk === false || $chunk === '') {
            return;
        }
        $buffer .= $chunk;
        while (($position = strpos($buffer, "\n")) !== false) {
            $line = rtrim(substr($buffer, 0, $position), "\r");
            $buffer = substr($buffer, $position + 1);
            $lines[] = ['ms' => (hrtime(true) - $this->t0) / 1e6, 'line' => $line];
        }
    }
}

/**
 * @param array<string, string> $env
 * @param list<string> $detail
 */
function runSession(int $number, Tk $monitor, array $env, string $root, array &$detail): array
{
    $run = new TimingRun($monitor, $env, $root);
    $run->start();
    try {
        $run->discover();
        $startup = $run->measureStartup();
        if ($startup['depth'] !== '10000') {
            fail('startup depth is ' . $startup['depth'] . ', expected 10000');
        }
        printf("run %d startup: %.1f ms, depth %s\n", $number, $startup['ms'], $startup['depth']);
        $run->settleInitialFrame();

        $steps = [];
        for ($i = 1; $i <= ZOOM_STEPS; $i++) {
            $step = $run->measureStep('::mandel::zoomIn');
            $expected = sprintf('%.10g', 10000.0 * (2 ** $i));
            if ($step['depth'] !== $expected) {
                fail('zoom ' . $i . ' depth is ' . $step['depth'] . ', expected ' . $expected);
            }
            $steps['zoom ' . $i] = $step;
        }

        $reset = $run->measureStep('::mandel::reset');
        if ($reset['depth'] !== '10000') {
            fail('reset depth is ' . $reset['depth'] . ', expected 10000');
        }
        $steps['reset'] = $reset;

        $run->send(Layout::TOOLBAR . '.re delete 0 end; ' . Layout::TOOLBAR . '.re insert 0 {-0.743643887037151}');
        $run->send(Layout::TOOLBAR . '.im delete 0 end; ' . Layout::TOOLBAR . '.im insert 0 {0.13182590420533}');
        $run->send(Layout::TOOLBAR . '.depth delete 0 end; ' . Layout::TOOLBAR . '.depth insert 0 {1e5}');
        $apply = $run->measureStep('::mandel::apply');
        if ($apply['depth'] !== '100000') {
            fail('apply depth is ' . $apply['depth'] . ', expected 100000');
        }
        $steps['apply'] = $apply;

        $exit = $run->quit();
        if ($exit !== 0) {
            fail('app exit code is ' . $exit . ', expected 0');
        }

        $stderr = $run->stderrLines();
        $warnings = $run->warnings();
        $detail[] = sprintf('run %d startup: total=%.1f depth=%s transitions=%s', $number, $startup['ms'], $startup['depth'], json_encode($startup['transitions']));
        foreach ($steps as $label => $step) {
            $detail[] = sprintf(
                'run %d %s: preview(stderr)=%s status_preview=%s full(stderr)=%s status_full=%s stderr_full=%s preview_frame=%s full_frame=%d total=%.1f idle=%s depth=%s transitions=%s',
                $number,
                $label,
                $step['preview_ms'] === null ? 'n/a' : sprintf('%.1f', $step['preview_ms']),
                $step['status_preview_ms'] === null ? 'n/a' : sprintf('%.1f', $step['status_preview_ms']),
                sprintf('%.1f', $step['full_ms']),
                $step['status_full_ms'] === null ? 'n/a' : sprintf('%.1f', $step['status_full_ms']),
                sprintf('%.1f', $step['stderr_full_ms']),
                $step['preview_frame'] === null ? 'n/a' : (string) $step['preview_frame'],
                $step['stderr_frame'],
                $step['total_ms'],
                $step['idle_ms'] === null ? 'n/a' : sprintf('%.1f', $step['idle_ms']),
                $step['depth'],
                json_encode($step['transitions']),
            );
        }
        foreach ($stderr as $entry) {
            $detail[] = sprintf('run %d stderr %.1f ms: %s', $number, $entry['ms'], $entry['line']);
        }

        return [
            'startup' => $startup,
            'steps' => $steps,
            'exit' => $exit,
            'stderr' => $stderr,
            'warnings' => $warnings,
        ];
    } finally {
        $run->finish();
    }
}

$scriptStart = hrtime(true);
$exitCode = 0;
$xvfb = null;
$display = null;
$monitor = null;
$detail = [];
$runs = [];

try {
    foreach ([':98', ':97', ':96', ':95', ':94'] as $candidate) {
        if (!displayUp($candidate)) {
            $display = $candidate;
            break;
        }
    }
    if ($display === null) {
        fail('no free X display for the timing run');
    }
    putenv('DISPLAY=' . $display);
    $xvfbDescriptors = [
        0 => ['file', '/dev/null', 'r'],
        1 => ['file', '/dev/null', 'w'],
        2 => ['file', '/dev/null', 'w'],
    ];
    $xvfb = proc_open(
        ['Xvfb', $display, '-screen', '0', '1400x900x24', '-nolisten', 'tcp'],
        $xvfbDescriptors,
        $xvfbPipes,
    );
    if (!is_resource($xvfb)) {
        fail('cannot start Xvfb on ' . $display);
    }
    $deadline = microtime(true) + 10.0;
    do {
        usleep(100_000);
        if (displayUp($display)) {
            break;
        }
    } while (microtime(true) < $deadline);
    if (!displayUp($display)) {
        fail('Xvfb on ' . $display . ' did not become ready');
    }
    echo 'timing display: ', $display, "\n";

    $monitor = new Tk();
    $monitor->open(320, 240);
    $monitor->evaluate('wm withdraw .');

    $env = getenv();
    $env['DISPLAY'] = $display;

    for ($i = 1; $i <= RUNS; $i++) {
        $runs[] = runSession($i, $monitor, $env, $root, $detail);
    }

    $stepLabels = ['startup'];
    for ($i = 1; $i <= ZOOM_STEPS; $i++) {
        $stepLabels[] = 'zoom ' . $i;
    }
    $stepLabels[] = 'reset';
    $stepLabels[] = 'apply';

    $table = [];
    $table[] = '| step | depth | preview ms (stderr preview wall, min/med/max) | full ms (stderr full wall, min/med/max) | total ms (min/med/max) |';
    $table[] = '| --- | --- | --- | --- | --- |';
    $startupValues = [];
    foreach ($runs as $run) {
        $startupValues[] = $run['startup']['ms'];
    }
    $table[] = sprintf(
        '| startup to first ready | %s | n/a | n/a | %s |',
        $runs[0]['startup']['depth'],
        statsCell($startupValues),
    );

    foreach (array_slice($stepLabels, 1) as $label) {
        $preview = [];
        $full = [];
        $total = [];
        $depth = '';
        foreach ($runs as $run) {
            $step = $run['steps'][$label];
            $depth = $step['depth'];
            if ($step['preview_ms'] !== null) {
                $preview[] = $step['preview_ms'];
            }
            if ($step['full_ms'] !== null) {
                $full[] = $step['full_ms'];
            }
            $total[] = $step['total_ms'];
        }
        $table[] = sprintf(
            '| %s | %s | %s | %s | %s |',
            $label,
            $depth,
            statsCell($preview),
            statsCell($full),
            statsCell($total),
        );
    }

    echo "\n", implode("\n", $table), "\n\n";
    echo "Cells are min / median / max over ", RUNS, " runs, in milliseconds.\n";
    echo "preview = wall= from the matching stderr kind=preview line; status-transition preview is in the detail log.\n";
    echo "full = wall= from the matching stderr kind=full line; total = send -> final ready.\n";
    echo "total also contains the ~150 ms idle gap before the full refine and the busy-state/Tk overhead.\n";
    echo 'startup = app process start -> first ready (initial preview frame), separate row.', "\n";
    echo 'quit exit codes: ';
    foreach ($runs as $index => $run) {
        echo ($index > 0 ? ', ' : ''), 'run ', $index + 1, '=', $run['exit'];
    }
    echo "\n";

    $warnings = [];
    foreach ($runs as $run) {
        foreach ($run['warnings'] as $entry) {
            $warnings[] = $entry['line'];
        }
    }
    if ($warnings !== []) {
        fail('app stderr contains warnings: ' . implode(' | ', $warnings));
    }

    $stderrCount = 0;
    foreach ($runs as $run) {
        $stderrCount += count($run['stderr']);
    }
    echo 'stderr frame-stat lines captured: ', $stderrCount, ' (detail: tests/gui/out/timing-detail.log)', "\n";
    echo "run 1 stderr sample with timestamps:\n";
    foreach ($runs[0]['stderr'] as $entry) {
        if (preg_match('/^frame \d+ kind=(preview|full) /', $entry['line']) === 1) {
            printf("  %.1f ms  %s\n", $entry['ms'], $entry['line']);
        }
    }

    if (!is_dir(OUT_DIR)) {
        mkdir(OUT_DIR, 0o777, true);
    }
    file_put_contents(OUT_DIR . '/timing-table.md', implode("\n", $table) . "\n");
    file_put_contents(OUT_DIR . '/timing-detail.log', implode("\n", $detail) . "\n");
    printf("total harness wall time: %.1f s\n", (hrtime(true) - $scriptStart) / 1e9);
    echo "PASS: timing harness\n";
} catch (Throwable $error) {
    $exitCode = 1;
    fwrite(STDERR, 'FAIL: ' . $error->getMessage() . "\n");
    if ($detail !== []) {
        fwrite(STDERR, "detail so far:\n" . implode("\n", $detail) . "\n");
    }
    if (!is_dir(OUT_DIR)) {
        @mkdir(OUT_DIR, 0o777, true);
    }
    @file_put_contents(OUT_DIR . '/timing-detail.log', implode("\n", $detail) . "\n");
} finally {
    if ($monitor !== null) {
        try {
            $monitor->close();
        } catch (Throwable) {
        }
    }
    if ($xvfb !== null) {
        $status = proc_get_status($xvfb);
        if ($status['running']) {
            proc_terminate($xvfb, 15);
            $deadline = microtime(true) + 5.0;
            while (microtime(true) < $deadline) {
                usleep(50_000);
                $status = proc_get_status($xvfb);
                if (!$status['running']) {
                    break;
                }
            }
            if ($status['running']) {
                proc_terminate($xvfb, 9);
            }
        }
        proc_close($xvfb);
        echo 'Xvfb ', $display, " stopped\n";
    }
}

exit($exitCode);
