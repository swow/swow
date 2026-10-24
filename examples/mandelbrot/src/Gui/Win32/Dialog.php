<?php

declare(strict_types=1);

namespace Swow\Mandelbrot\Gui\Win32;

use FFI;

/**
 * Native save dialog for the PNG export.
 *
 * Owns the comdlg32 import so the backend does not need it.
 */
final class Dialog
{
    private const SAVE_PATH_CHARS = 32768;

    /* OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_EXPLORER */
    private const SAVE_FLAGS = 0x0008080A;

    private const COMDLG32_CDEF = <<<'C'
    typedef struct {
        uint32_t lStructSize;
        intptr_t hwndOwner;
        intptr_t hInstance;
        uint16_t *lpstrFilter;
        uint16_t *lpstrCustomFilter;
        uint32_t nMaxCustFilter;
        uint32_t nFilterIndex;
        uint16_t *lpstrFile;
        uint32_t nMaxFile;
        uint16_t *lpstrFileTitle;
        uint32_t nMaxFileTitle;
        uint16_t *lpstrInitialDir;
        uint16_t *lpstrTitle;
        uint32_t Flags;
        uint16_t nFileOffset;
        uint16_t nFileExtension;
        uint16_t *lpstrDefExt;
        intptr_t lCustData;
        intptr_t lpfnHook;
        uint16_t *lpTemplateName;
        intptr_t pvReserved;
        uint32_t dwReserved;
        uint32_t FlagsEx;
    } OPENFILENAMEW;
    int GetSaveFileNameW(OPENFILENAMEW *openFileName);
    C;

    private FFI $comdlg32;

    public function __construct()
    {
        $this->comdlg32 = FFI::cdef(self::COMDLG32_CDEF, 'comdlg32.dll');
    }

    /**
     * Ask for a PNG path through GetSaveFileNameW.
     *
     * Returns null when the user cancels or selects nothing.
     */
    public function askSavePath(int $owner, int $instance, string $default): ?string
    {
        $name = $default !== '' ? $default : 'mandelbrot.png';
        $filterBytes = Text::toUtf16Le('PNG image (*.png)') . "\0\0"
            . Text::toUtf16Le('*.png') . "\0\0"
            . Text::toUtf16Le('All files (*.*)') . "\0\0"
            . Text::toUtf16Le('*.*') . "\0\0"
            . "\0\0";
        $filter = $this->comdlg32->new(
            'uint16_t[' . intdiv(strlen($filterBytes), 2) . ']',
        );
        FFI::memcpy($filter, $filterBytes, strlen($filterBytes));

        $file = $this->comdlg32->new('uint16_t[' . self::SAVE_PATH_CHARS . ']');
        FFI::memset($file, 0, self::SAVE_PATH_CHARS * 2);
        $nameBytes = Text::toUtf16Le($name) . "\0\0";
        FFI::memcpy($file, $nameBytes, strlen($nameBytes));

        $defExt = Text::wideFor($this->comdlg32, 'png');
        $title = Text::wideFor($this->comdlg32, 'Save Mandelbrot PNG');

        $openFileName = $this->comdlg32->new('OPENFILENAMEW');
        $openFileName->lStructSize = FFI::sizeof($openFileName);
        $openFileName->hwndOwner = $owner;
        $openFileName->hInstance = $instance;
        $openFileName->lpstrFilter = $this->comdlg32->cast('uint16_t *', $filter);
        $openFileName->nFilterIndex = 1;
        $openFileName->lpstrFile = $this->comdlg32->cast('uint16_t *', $file);
        $openFileName->nMaxFile = self::SAVE_PATH_CHARS;
        $openFileName->lpstrTitle = $this->comdlg32->cast('uint16_t *', $title);
        $openFileName->Flags = self::SAVE_FLAGS;
        $openFileName->lpstrDefExt = $this->comdlg32->cast('uint16_t *', $defExt);

        if ($this->comdlg32->GetSaveFileNameW(FFI::addr($openFileName)) === 0) {
            return null;
        }

        $path = Text::fromUtf16Le(FFI::string($file, self::SAVE_PATH_CHARS * 2));

        return $path === '' ? null : $path;
    }
}
