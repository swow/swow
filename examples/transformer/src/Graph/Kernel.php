<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Graph;

interface Kernel
{
    /**
     * @param list<Tensor> $inputs
     * @param array<string, mixed> $attrs
     * @return list<float>|list<int>
     */
    public function execute(array $inputs, array $attrs): array;
}
