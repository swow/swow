<?php

declare(strict_types=1);

namespace Swow\Mandelbrot\Gui;

use const PHP_OS_FAMILY;

/**
 * Pick the native GUI backend for the current platform.
 *
 * Win32 is optional: another source file may not exist yet, so it is only
 * referenced when the class is already loadable.
 */
final class Factory
{
    public static function create(): Backend
    {
        return match (PHP_OS_FAMILY) {
            'Linux' => new Tk(),
            'Darwin' => new Cocoa(),
            'Windows' => self::windows(),
            default => new Tk(),
        };
    }

    private static function windows(): Backend
    {
        if (class_exists(Win32::class)) {
            return new Win32();
        }

        return new Tk();
    }
}
