<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Chat;

use RuntimeException;

final class ContextLimitException extends RuntimeException
{
    public function __construct(
        public readonly int $position,
        public readonly int $needed,
        public readonly int $limit,
    ) {
        parent::__construct("Context limit reached: {$position} + {$needed} > {$limit}");
    }
}
