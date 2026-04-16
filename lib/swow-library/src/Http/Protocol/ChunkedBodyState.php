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

namespace Swow\Http\Protocol;

use Swow\Buffer;
use Swow\Http\Parser as HttpParser;

final class ChunkedBodyState
{
    /** 当前 chunk 的声明长度，用于补齐跨包 body 读取 */
    public int $currentChunkLength = 0;

    /** 当前 chunk 中尚未从 socket 读取的剩余 body 字节数（增量读取大 chunk 时 > 0） */
    public int $remainingChunkBytes = 0;

    /** 是否已经读到 HTTP MESSAGE_COMPLETE */
    public bool $complete = false;

    /** 是否已把 parser/buffer 状态回收并提交给连接 */
    public bool $finalized = false;

    public function __construct(
        public Buffer $buffer,
        public HttpParser $parser,
        public int $parsedOffset,
        public Buffer $bodyBuffer,
    ) {
    }
}
