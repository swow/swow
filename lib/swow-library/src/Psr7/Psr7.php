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

namespace Swow\Psr7;

use Swow\Psr7\Utils\ConverterTrait;
use Swow\Psr7\Utils\CreatorTrait;
use Swow\Psr7\Utils\DetectorTrait;
use Swow\Psr7\Utils\FactoryTrait;
use Swow\Psr7\Utils\OperatorTrait;

class Psr7
{
    use ConverterTrait;

    use CreatorTrait;

    use DetectorTrait;

    use FactoryTrait;

    use OperatorTrait;
}
