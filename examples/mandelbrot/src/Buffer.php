<?php

declare(strict_types=1);

namespace Swow\Mandelbrot;

use InvalidArgumentException;
use Swow\SharedBuffer;

use function array_key_exists;
use function count;
use function in_array;
use function is_int;

use const PHP_INT_MAX;

final class Buffer
{
    public const MAGIC = 0x4D4E444C;
    public const LAYOUT_VERSION = 2;
    public const HEADER_SIZE = 64;
    public const PREVIEW_STEP = 4;
    public const DEFAULT_MAX_LIMIT = 65536;
    public const BAND_ROWS = 8;
    public const GLITCH_SLOTS = 1024;

    private const PIXEL_SIZE = 4;
    private const GLITCH_RECORD_HEADER = 8;
    private const GLITCH_RECORD_SIZE = self::GLITCH_RECORD_HEADER + self::GLITCH_SLOTS * 8;

    private const U64_FIELDS = ['frame', 'flags', 'reserved'];

    private function __construct(
        private readonly SharedBuffer $sharedBuffer,
        private readonly int $fullWidth,
        private readonly int $fullHeight,
        private readonly int $previewWidth,
        private readonly int $previewHeight,
        private readonly int $bandCount,
        private readonly int $maxLimit,
        private readonly int $fullOffset,
        private readonly int $previewOffset,
        private readonly int $orbitRealOffset,
        private readonly int $orbitImagOffset,
        private readonly int $orbitMagOffset,
        private readonly int $glitchTableOffset,
    ) {
    }

    public static function create(int $width, int $height, int $maxLimit = self::DEFAULT_MAX_LIMIT): self
    {
        if ($width < 4 || $height < 4) {
            throw new InvalidArgumentException('width and height must be at least 4');
        }
        if ($maxLimit < 1) {
            throw new InvalidArgumentException('maxLimit must be at least 1');
        }

        $previewWidth = intdiv($width, self::PREVIEW_STEP);
        $previewHeight = intdiv($height, self::PREVIEW_STEP);
        $bandCount = intdiv($height + self::BAND_ROWS - 1, self::BAND_ROWS);
        $fullOffset = self::HEADER_SIZE;
        $previewOffset = self::align64($fullOffset + $width * $height * self::PIXEL_SIZE);
        $orbitRealOffset = self::align64(
            $previewOffset + $previewWidth * $previewHeight * self::PIXEL_SIZE,
        );
        $orbitLength = $maxLimit + 1;
        $orbitImagOffset = $orbitRealOffset + $orbitLength * 8;
        $orbitMagOffset = $orbitImagOffset + $orbitLength * 8;
        $glitchTableOffset = self::align64($orbitMagOffset + $orbitLength * 8);
        $size = $glitchTableOffset + $bandCount * self::GLITCH_RECORD_SIZE;

        $buffer = new self(
            new SharedBuffer($size),
            $width,
            $height,
            $previewWidth,
            $previewHeight,
            $bandCount,
            $maxLimit,
            $fullOffset,
            $previewOffset,
            $orbitRealOffset,
            $orbitImagOffset,
            $orbitMagOffset,
            $glitchTableOffset,
        );
        $buffer->writeHeader([
            'magic' => self::MAGIC,
            'layoutVersion' => self::LAYOUT_VERSION,
            'fullWidth' => $width,
            'fullHeight' => $height,
            'limit' => $maxLimit,
            'refVersion' => 0,
            'frame' => 0,
            'flags' => 0,
            'previewWidth' => $previewWidth,
            'previewHeight' => $previewHeight,
            'glitchTableOffset' => $glitchTableOffset,
            'glitchSlotsPerBand' => self::GLITCH_SLOTS,
            'reserved' => 0,
        ]);

        return $buffer;
    }

    public function serializeHandle(): string
    {
        return serialize($this->sharedBuffer);
    }

    /**
     * @param array<string, int> $fields
     */
    public function writeHeader(array $fields): void
    {
        if ($fields === []) {
            return;
        }

        $header = $this->readHeader();
        foreach ($fields as $name => $value) {
            if (!array_key_exists($name, $header)) {
                throw new InvalidArgumentException("unknown header field: {$name}");
            }
            if (!is_int($value)) {
                throw new InvalidArgumentException("header field {$name} must be an integer");
            }

            $max = in_array($name, self::U64_FIELDS, true) ? PHP_INT_MAX : 0xFFFFFFFF;
            if ($value < 0 || $value > $max) {
                throw new InvalidArgumentException("header field {$name} is out of range");
            }
            if ($name === 'limit' && $value > $this->maxLimit) {
                throw new InvalidArgumentException("header limit exceeds maxLimit {$this->maxLimit}");
            }

            $header[$name] = $value;
        }

        $this->sharedBuffer->write(0, pack(
            'V6P2V4P',
            $header['magic'],
            $header['layoutVersion'],
            $header['fullWidth'],
            $header['fullHeight'],
            $header['limit'],
            $header['refVersion'],
            $header['frame'],
            $header['flags'],
            $header['previewWidth'],
            $header['previewHeight'],
            $header['glitchTableOffset'],
            $header['glitchSlotsPerBand'],
            $header['reserved'],
        ));
    }

    /**
     * @return array<string, int>
     */
    public function readHeader(): array
    {
        return unpack(
            'Vmagic/VlayoutVersion/VfullWidth/VfullHeight/Vlimit/VrefVersion'
                . '/Pframe/Pflags/VpreviewWidth/VpreviewHeight'
                . '/VglitchTableOffset/VglitchSlotsPerBand/Preserved',
            $this->sharedBuffer->read(0, self::HEADER_SIZE),
        );
    }

    public function fullWidth(): int
    {
        return $this->fullWidth;
    }

    public function fullHeight(): int
    {
        return $this->fullHeight;
    }

    public function previewWidth(): int
    {
        return $this->previewWidth;
    }

    public function previewHeight(): int
    {
        return $this->previewHeight;
    }

    public function previewStep(): int
    {
        return self::PREVIEW_STEP;
    }

    public function bandCount(): int
    {
        return $this->bandCount;
    }

    public function glitchSlotsPerBand(): int
    {
        return self::GLITCH_SLOTS;
    }

    /**
     * @param list<int> $colors target image row, 0x00RRGGBB per pixel
     */
    public function writeRows(bool $preview, int $rowBegin, array $colors): void
    {
        [$width, $height, $offset] = $this->region($preview);
        if ($rowBegin < 0 || $rowBegin >= $height) {
            throw new InvalidArgumentException("row out of range: {$rowBegin}");
        }
        if (count($colors) !== $width) {
            throw new InvalidArgumentException("row must contain {$width} pixels");
        }

        $this->sharedBuffer->writeU32(
            $offset + $rowBegin * $width * self::PIXEL_SIZE,
            $this->packColors($colors),
            SharedBuffer::LITTLE_ENDIAN,
        );
    }

    /**
     * @param list<int> $colors consecutive rows, row-major, 0x00RRGGBB per pixel
     */
    public function writeBand(bool $preview, int $rowBegin, array $colors): void
    {
        [$width, $height, $offset] = $this->region($preview);
        $count = count($colors);
        if ($count % $width !== 0) {
            throw new InvalidArgumentException('band length must be a multiple of the row width');
        }

        $rows = intdiv($count, $width);
        if ($rowBegin < 0 || $rowBegin + $rows > $height) {
            throw new InvalidArgumentException("band rows out of range: {$rowBegin} + {$rows}");
        }
        if ($rows === 0) {
            return;
        }

        $this->sharedBuffer->writeU32(
            $offset + $rowBegin * $width * self::PIXEL_SIZE,
            $this->packColors($colors),
            SharedBuffer::LITTLE_ENDIAN,
        );
    }

    public function writePixel(bool $preview, int $x, int $y, int $color): void
    {
        [$width, $height, $offset] = $this->region($preview);
        if ($x < 0 || $x >= $width || $y < 0 || $y >= $height) {
            throw new InvalidArgumentException("pixel out of range: {$x},{$y}");
        }

        $this->sharedBuffer->writeU32(
            $offset + ($y * $width + $x) * self::PIXEL_SIZE,
            [0xFF000000 | ($color & 0xFFFFFF)],
            SharedBuffer::LITTLE_ENDIAN,
        );
    }

    public function readRows(bool $preview, int $rowBegin, int $rowEnd): string
    {
        [$width, $height, $offset] = $this->region($preview);
        if ($rowBegin < 0 || $rowEnd < $rowBegin || $rowEnd > $height) {
            throw new InvalidArgumentException("row range out of range: {$rowBegin}..{$rowEnd}");
        }

        return $this->sharedBuffer->read(
            $offset + $rowBegin * $width * self::PIXEL_SIZE,
            ($rowEnd - $rowBegin) * $width * self::PIXEL_SIZE,
        );
    }

    public function readAll(bool $preview): string
    {
        [$width, $height, $offset] = $this->region($preview);

        return $this->sharedBuffer->read($offset, $width * $height * self::PIXEL_SIZE);
    }

    /**
     * @param list<float> $zr
     * @param list<float> $zi
     * @param list<float> $mag
     */
    public function writeOrbit(array $zr, array $zi, array $mag): void
    {
        $length = $this->activeLimit() + 1;
        foreach ([$zr, $zi, $mag] as $values) {
            if (count($values) !== $length) {
                throw new InvalidArgumentException("orbit must contain exactly {$length} values");
            }
        }

        $this->sharedBuffer->writeF64($this->orbitRealOffset, array_values($zr), SharedBuffer::LITTLE_ENDIAN);
        $this->sharedBuffer->writeF64($this->orbitImagOffset, array_values($zi), SharedBuffer::LITTLE_ENDIAN);
        $this->sharedBuffer->writeF64($this->orbitMagOffset, array_values($mag), SharedBuffer::LITTLE_ENDIAN);
    }

    /**
     * @return array{zr: list<float>, zi: list<float>, mag: list<float>}
     */
    public function readOrbit(): array
    {
        $length = $this->activeLimit() + 1;

        return [
            'zr' => $this->sharedBuffer->viewF64($this->orbitRealOffset, $length, SharedBuffer::LITTLE_ENDIAN),
            'zi' => $this->sharedBuffer->viewF64($this->orbitImagOffset, $length, SharedBuffer::LITTLE_ENDIAN),
            'mag' => $this->sharedBuffer->viewF64($this->orbitMagOffset, $length, SharedBuffer::LITTLE_ENDIAN),
        ];
    }

    /**
     * Store the first glitch pairs of a band. Returns the number stored.
     *
     * @param list<int> $glitches flat [x, y, ...] in image coordinates
     */
    public function writeGlitches(int $bandIndex, array $glitches): int
    {
        $offset = $this->glitchRecordOffset($bandIndex);
        $pairs = intdiv(count($glitches), 2);
        $stored = min($pairs, self::GLITCH_SLOTS);

        $words = [$stored, 0];
        for ($i = 0; $i < $stored * 2; $i++) {
            $words[] = $glitches[$i];
        }

        $this->sharedBuffer->writeU32($offset, $words, SharedBuffer::LITTLE_ENDIAN);

        return $stored;
    }

    /**
     * @return array{count: int, glitches: list<int>}
     */
    public function readGlitches(int $bandIndex): array
    {
        $offset = $this->glitchRecordOffset($bandIndex);
        $count = $this->sharedBuffer->viewU32($offset, 1, SharedBuffer::LITTLE_ENDIAN)[0];
        $stored = min($count, self::GLITCH_SLOTS);
        $glitches = $stored > 0
            ? $this->sharedBuffer->viewU32(
                $offset + self::GLITCH_RECORD_HEADER,
                $stored * 2,
                SharedBuffer::LITTLE_ENDIAN,
            )
            : [];

        return ['count' => $stored, 'glitches' => $glitches];
    }

    private function glitchRecordOffset(int $bandIndex): int
    {
        if ($bandIndex < 0 || $bandIndex >= $this->bandCount) {
            throw new InvalidArgumentException("band out of range: {$bandIndex}");
        }

        return $this->glitchTableOffset + $bandIndex * self::GLITCH_RECORD_SIZE;
    }

    private function activeLimit(): int
    {
        return $this->readHeader()['limit'];
    }

    /**
     * @return array{int, int, int} width, height and byte offset of a region
     */
    private function region(bool $preview): array
    {
        return $preview
            ? [$this->previewWidth, $this->previewHeight, $this->previewOffset]
            : [$this->fullWidth, $this->fullHeight, $this->fullOffset];
    }

    /**
     * @param list<int> $colors
     * @return list<int> little-endian words with the alpha byte set
     */
    private function packColors(array $colors): array
    {
        $pixels = [];
        foreach ($colors as $color) {
            $pixels[] = 0xFF000000 | ($color & 0xFFFFFF);
        }

        return $pixels;
    }

    private static function align64(int $value): int
    {
        return ($value + 63) & ~63;
    }
}
