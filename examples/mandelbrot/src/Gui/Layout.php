<?php
declare(strict_types=1);

namespace Swow\Mandelbrot\Gui;

enum NodeKind
{
    case Leaf;
    case VBox;
    case HBox;
}

enum LeafKind: string
{
    case Label = 'label';
    case Edit = 'edit';
    case Button = 'button';
    case Spacer = 'spacer';
}

enum Valign
{
    case Stretch;
    case Center;
    case Top;
    case Bottom;
}

/** Rectangle in DIP, relative to the window client area. */
final class Rect
{
    public function __construct(
        public readonly int $x,
        public readonly int $y,
        public readonly int $w,
        public readonly int $h,
    ) {
    }
}

/**
 * Natural sizes and spacing for one platform, in DIP.
 *
 * $sizes overrides the per-kind defaults for one leaf. Edits and buttons are
 * keyed by component; labels without a component are keyed as label:<text>.
 */
final class Metrics
{
    /**
     * @param array<string, array{0: float, 1: float}> $sizes
     */
    public function __construct(
        public readonly float $gapX = 4.0,
        public readonly float $gapY = 1.0,
        public readonly float $marginX = 2.0,
        public readonly float $marginY = 0.0,
        public readonly float $labelWidth = 45.0,
        public readonly float $labelHeight = 25.0,
        public readonly float $editWidth = 200.0,
        public readonly float $editHeight = 27.0,
        public readonly float $buttonWidth = 50.0,
        public readonly float $buttonHeight = 27.0,
        public readonly float $statusHeight = 16.0,
        public readonly array $sizes = [],
    ) {
    }

    /**
     * @return array{0: float, 1: float} width and height of one leaf, in DIP
     */
    public function sizeFor(?string $component, LeafKind $kind, string $text): array
    {
        $key = $component ?? 'label:' . $text;
        if (isset($this->sizes[$key])) {
            return $this->sizes[$key];
        }

        return match ($kind) {
            LeafKind::Label => [
                $this->labelWidth,
                $component === Layout::COMP_STATUS ? $this->statusHeight : $this->labelHeight,
            ],
            LeafKind::Edit => [$this->editWidth, $this->editHeight],
            LeafKind::Button => [$this->buttonWidth, $this->buttonHeight],
            LeafKind::Spacer => [0.0, 0.0],
        };
    }
}

/**
 * One node of the platform-neutral layout tree.
 *
 * Leaves carry a component name (or null for decoration) and a key used in the
 * resolved rect map. Containers carry children and the grow factors.
 */
final class Node
{
    /**
     * @param list<Node> $children
     */
    private function __construct(
        public readonly NodeKind $kind,
        public readonly ?string $component,
        public readonly LeafKind $leaf,
        public readonly string $text,
        public readonly string $key,
        public readonly array $children = [],
        public float $growX = 0.0,
        public float $growY = 0.0,
        public Valign $valign = Valign::Stretch,
    ) {
    }

    public static function label(string $text, ?string $component = null): self
    {
        return new self(
            NodeKind::Leaf,
            $component,
            LeafKind::Label,
            $text,
            $component ?? 'label:' . $text,
            [],
            0.0,
            0.0,
            Valign::Center,
        );
    }

    public static function edit(string $component): self
    {
        return new self(NodeKind::Leaf, $component, LeafKind::Edit, '', $component);
    }

    public static function button(string $component, string $text): self
    {
        return new self(NodeKind::Leaf, $component, LeafKind::Button, $text, $component);
    }

    public static function spacer(): self
    {
        return new self(NodeKind::Leaf, null, LeafKind::Spacer, '', '', [], 1.0, 1.0);
    }

    public static function vbox(self ...$children): self
    {
        return new self(NodeKind::VBox, null, LeafKind::Spacer, '', '', $children);
    }

    public static function hbox(self ...$children): self
    {
        return new self(NodeKind::HBox, null, LeafKind::Spacer, '', '', $children);
    }

    public function growX(float $factor): self
    {
        $this->growX = $factor;

        return $this;
    }

    public function growY(float $factor): self
    {
        $this->growY = $factor;

        return $this;
    }

    public function valign(Valign $valign): self
    {
        $this->valign = $valign;

        return $this;
    }
}

/**
 * Fixed window layout for the demo.
 *
 * The tree describes the toolbar content; the image area is separate and is
 * derived from the toolbar height, as layout.md requires. Tk is the only
 * backend today, so metrics() carries the Tk natural sizes measured at 96 DPI.
 */
final class Layout
{
    public const TITLE = 'Swow Mandelbrot';
    public const TOOLBAR_HEIGHT = 44;

    public const FULL_IMAGE = 'mandel_full';
    public const PREVIEW_IMAGE = 'mandel_preview';
    public const PLACEHOLDER_IMAGE = 'placeholder';
    public const PLACEHOLDER_TAG = 'placeholder';

    public const COMP_REAL = 'Real';
    public const COMP_IMAG = 'Imag';
    public const COMP_DEPTH = 'Depth';
    public const COMP_ZOOM_OUT = 'ZoomOut';
    public const COMP_ZOOM_IN = 'ZoomIn';
    public const COMP_RESET = 'Reset';
    public const COMP_GO = 'Go';
    public const COMP_SAVE = 'Save';
    public const COMP_QUIT = 'Quit';
    public const COMP_PAN_UP = 'PanUp';
    public const COMP_PAN_DOWN = 'PanDown';
    public const COMP_PAN_LEFT = 'PanLeft';
    public const COMP_PAN_RIGHT = 'PanRight';
    public const COMP_STATUS = 'Status';
    public const COMP_CANVAS = 'Canvas';

    public const COMPONENTS = [
        self::COMP_REAL,
        self::COMP_IMAG,
        self::COMP_DEPTH,
        self::COMP_ZOOM_OUT,
        self::COMP_ZOOM_IN,
        self::COMP_RESET,
        self::COMP_GO,
        self::COMP_SAVE,
        self::COMP_QUIT,
        self::COMP_PAN_UP,
        self::COMP_PAN_DOWN,
        self::COMP_PAN_LEFT,
        self::COMP_PAN_RIGHT,
        self::COMP_STATUS,
        self::COMP_CANVAS,
    ];

    public const TOOLBAR = '.toolbar';
    public const STATUS = '.toolbar.status';
    public const CANVAS = '.canvas';

    public const ENTRY_RE = '.toolbar.re';
    public const ENTRY_IM = '.toolbar.im';
    public const ENTRY_DEPTH = '.toolbar.depth';

    public const BUTTON_ZOOM_OUT = '.toolbar.zoomOut';
    public const BUTTON_ZOOM_IN = '.toolbar.zoomIn';
    public const BUTTON_RESET = '.toolbar.reset';
    public const BUTTON_GO = '.toolbar.go';
    public const BUTTON_SAVE = '.toolbar.save';
    public const BUTTON_QUIT = '.toolbar.quit';
    public const BUTTON_PAN_UP = '.toolbar.panUp';
    public const BUTTON_PAN_DOWN = '.toolbar.panDown';
    public const BUTTON_PAN_LEFT = '.toolbar.panLeft';
    public const BUTTON_PAN_RIGHT = '.toolbar.panRight';

    public const CMD_ZOOM_OUT = '::mandel::zoomOut';
    public const CMD_ZOOM_IN = '::mandel::zoomIn';
    public const CMD_RESET = '::mandel::reset';
    public const CMD_APPLY = '::mandel::apply';
    public const CMD_SAVE = '::mandel::save';
    public const CMD_QUIT = '::mandel::quit';
    public const CMD_PAN_UP = '::mandel::panUp';
    public const CMD_PAN_DOWN = '::mandel::panDown';
    public const CMD_PAN_LEFT = '::mandel::panLeft';
    public const CMD_PAN_RIGHT = '::mandel::panRight';

    public static function tree(): Node
    {
        return Node::vbox(
            Node::hbox(
                Node::label('Real'),
                Node::edit(self::COMP_REAL),
                Node::label('Imag'),
                Node::edit(self::COMP_IMAG),
                Node::label('Depth'),
                Node::edit(self::COMP_DEPTH),
                Node::button(self::COMP_ZOOM_OUT, '-'),
                Node::button(self::COMP_ZOOM_IN, '+'),
                Node::button(self::COMP_RESET, 'Reset'),
                Node::button(self::COMP_GO, 'Go'),
                Node::button(self::COMP_SAVE, 'Save'),
                Node::button(self::COMP_QUIT, 'Quit'),
                Node::button(self::COMP_PAN_UP, '↑'),
                Node::button(self::COMP_PAN_DOWN, '↓'),
                Node::button(self::COMP_PAN_LEFT, '←'),
                Node::button(self::COMP_PAN_RIGHT, '→'),
            ),
            Node::label('', self::COMP_STATUS),
        );
    }

    /** Absolute path of the placeholder image, relative to this source file. */
    public static function placeholderPath(): string
    {
        return dirname(__DIR__, 2) . '/assets/placeholder.png';
    }

    /**
     * Tk metrics, in DIP. Natural sizes reproduce the previous pack layout.
     */
    public static function metrics(): Metrics
    {
        return new Metrics(
            gapX: 4.0,
            gapY: 1.0,
            marginX: 2.0,
            marginY: 0.0,
            labelWidth: 45.0,
            labelHeight: 25.0,
            editWidth: 200.0,
            editHeight: 27.0,
            buttonWidth: 50.0,
            buttonHeight: 27.0,
            statusHeight: 16.0,
            sizes: [
                'label:Real' => [33.0, 25.0],
                'label:Imag' => [37.0, 25.0],
                'label:Depth' => [45.0, 25.0],
                self::COMP_REAL => [246.0, 27.0],
                self::COMP_IMAG => [246.0, 27.0],
                self::COMP_DEPTH => [102.0, 27.0],
                self::COMP_ZOOM_OUT => [35.0, 27.0],
                self::COMP_ZOOM_IN => [38.0, 27.0],
                self::COMP_RESET => [67.0, 27.0],
                self::COMP_GO => [49.0, 27.0],
                self::COMP_SAVE => [62.0, 27.0],
                self::COMP_QUIT => [58.0, 27.0],
                self::COMP_PAN_UP => [28.0, 27.0],
                self::COMP_PAN_DOWN => [28.0, 27.0],
                self::COMP_PAN_LEFT => [28.0, 27.0],
                self::COMP_PAN_RIGHT => [28.0, 27.0],
            ],
        );
    }

    /** Height of the toolbar content, including the metrics margins. */
    public static function toolbarHeight(Metrics $metrics): int
    {
        return self::heightOf(self::tree(), $metrics);
    }

    /**
     * Resolve a window to one rect per component, plus one rect per
     * non-component label (keyed as label:<text>).
     *
     * @return array<string, Rect>
     */
    public static function resolve(Node $root, Metrics $metrics, int $width, int $height): array
    {
        $toolbarHeight = self::heightOf($root, $metrics);
        if ($width - 2 * $metrics->marginX < 1 || $height <= $toolbarHeight) {
            throw new \InvalidArgumentException(sprintf(
                'window size %dx%d does not fit a %d pixel toolbar',
                $width,
                $height,
                $toolbarHeight,
            ));
        }

        $rects = [];
        self::place(
            $root,
            $metrics,
            $metrics->marginX,
            $metrics->marginY,
            $width - 2 * $metrics->marginX,
            $toolbarHeight - 2 * $metrics->marginY,
            $rects,
        );
        $rects[self::COMP_CANVAS] = new Rect(0, $toolbarHeight, $width, $height - $toolbarHeight);

        return $rects;
    }

    /**
     * @return array{0: float, 1: float} natural width and height
     */
    private static function measure(Node $node, Metrics $metrics): array
    {
        if ($node->kind === NodeKind::Leaf) {
            return $metrics->sizeFor($node->component, $node->leaf, $node->text);
        }

        $width = 0.0;
        $height = 0.0;
        foreach ($node->children as $index => $child) {
            [$childWidth, $childHeight] = self::measure($child, $metrics);
            if ($node->kind === NodeKind::VBox) {
                $width = max($width, $childWidth);
                $height += $childHeight;
                if ($index > 0) {
                    $height += $metrics->gapY;
                }
            } else {
                $width += $childWidth;
                if ($index > 0) {
                    $width += $metrics->gapX;
                }
                $height = max($height, $childHeight);
            }
        }

        return [$width, $height];
    }

    private static function heightOf(Node $root, Metrics $metrics): int
    {
        [, $height] = self::measure($root, $metrics);

        return (int) round($height + 2 * $metrics->marginY);
    }

    /**
     * @param array<string, Rect> $rects
     */
    private static function place(
        Node $node,
        Metrics $metrics,
        float $x,
        float $y,
        float $w,
        float $h,
        array &$rects,
    ): void {
        if ($node->kind === NodeKind::Leaf) {
            if ($node->key !== '') {
                $rects[$node->key] = new Rect(
                    (int) round($x),
                    (int) round($y),
                    (int) round($w),
                    (int) round($h),
                );
            }

            return;
        }

        if ($node->kind === NodeKind::VBox) {
            $fixed = 0.0;
            $grow = 0.0;
            foreach ($node->children as $child) {
                [, $childHeight] = self::measure($child, $metrics);
                if ($child->growY > 0) {
                    $grow += $child->growY;
                } else {
                    $fixed += $childHeight;
                }
            }
            $fixed += $metrics->gapY * max(0, count($node->children) - 1);
            $remaining = max($h - $fixed, 0.0);
            $cursor = $y;
            foreach ($node->children as $child) {
                [, $childHeight] = self::measure($child, $metrics);
                $allocated = $child->growY > 0
                    ? $remaining * ($child->growY / $grow)
                    : $childHeight;
                self::place($child, $metrics, $x, $cursor, $w, $allocated, $rects);
                $cursor += $allocated + $metrics->gapY;
            }

            return;
        }

        $fixed = 0.0;
        $grow = 0.0;
        foreach ($node->children as $child) {
            [$childWidth] = self::measure($child, $metrics);
            if ($child->growX > 0) {
                $grow += $child->growX;
            } else {
                $fixed += $childWidth;
            }
        }
        $fixed += $metrics->gapX * max(0, count($node->children) - 1);
        $remaining = max($w - $fixed, 0.0);
        $cursor = $x;
        foreach ($node->children as $child) {
            [$childWidth, $childHeight] = self::measure($child, $metrics);
            $allocated = $child->growX > 0
                ? $remaining * ($child->growX / $grow)
                : $childWidth;
            $childY = $y;
            $childBoxHeight = $h;
            switch ($child->valign) {
                case Valign::Center:
                    $childY = $y + ($h - $childHeight) / 2;
                    $childBoxHeight = $childHeight;
                    break;
                case Valign::Top:
                    $childBoxHeight = $childHeight;
                    break;
                case Valign::Bottom:
                    $childY = $y + $h - $childHeight;
                    $childBoxHeight = $childHeight;
                    break;
                case Valign::Stretch:
                    break;
            }
            self::place(
                $child,
                $metrics,
                $cursor,
                $childY,
                $allocated,
                $childBoxHeight,
                $rects,
            );
            $cursor += $allocated + $metrics->gapX;
        }
    }
}
