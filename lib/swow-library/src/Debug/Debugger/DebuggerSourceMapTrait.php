<?php

/**
 * This file is part of Swow
 *
 * @link    https://github.com/swow/swow
 * @contact twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);
/**
 * Author: Twosee <twose@qq.com>
 * Date: 2024/3/24 02:15
 */

namespace Swow\Debug\Debugger;

use function count;
use function file_exists;
use function file_get_contents;
use function getenv;
use function is_countable;
use function is_string;
use function json_decode;
use function str_replace;

use const JSON_THROW_ON_ERROR;

trait DebuggerSourceMapTrait
{
    /** @var callable|null */
    protected $sourceMapHandler;

    public function __constructDebuggerSourceMap(): void
    {
        $pathMap = getenv('SDB_SOURCE_MAP');
        if (is_string($pathMap) && file_exists($pathMap)) {
            $pathMap = json_decode(file_get_contents($pathMap), true, 512, JSON_THROW_ON_ERROR);
            $search = $replace = [];
            foreach ($pathMap as $key => $value) {
                $search[] = $key;
                $replace[] = $value;
            }
            if ((is_countable($pathMap) ? count($pathMap) : 0) > 0) {
                /* This can help you to see the real source position in the host machine in the terminal */
                $this->setSourceMapHandler(static function (string $sourcePosition) use ($search, $replace): string {
                    return str_replace($search, $replace, $sourcePosition);
                });
            }
        }
    }

    public function setSourceMapHandler(?callable $sourceMapHandler): static
    {
        $this->sourceMapHandler = $sourceMapHandler;

        return $this;
    }

    protected function callSourceMapHandler(string $sourcePosition): string
    {
        $sourceMapHandler = $this->sourceMapHandler ?? null;
        if ($sourceMapHandler !== null) {
            return $sourceMapHandler($sourcePosition);
        }

        return $sourcePosition;
    }
}
