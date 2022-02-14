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

class RawUploadedFile
{
    /** @var string client file name */
    public string $name;

    /** @var string mime type */
    public string $type;

    /** @var string temp file name */
    public string $tmp_name;

    /** @var int UPLOAD_ERR_* error code */
    public int $error;

    /** @var int The size of the file in bytes */
    public int $size;
}
