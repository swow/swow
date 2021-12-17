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

namespace Swow\Http;

trait TypeTrait
{
    protected int $type = self::TYPE_HTTP;

    protected function upgraded(int $type): void
    {
        $this->type = $type;
        /* release useless resources */
        // if (!($type & static::TYPE_HTTP)) {
        $this->httpParser = null;
        // }
    }
}
