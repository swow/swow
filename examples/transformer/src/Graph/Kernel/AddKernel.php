<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Graph\Kernel;

use Swow\Examples\Transformer\Graph\Kernel;

final class AddKernel implements Kernel
{
    public function execute(array $inputs, array $attrs): array
    {
        [$left, $right] = $inputs;
        $result = [];
        foreach ($left->data() as $index => $value) {
            $result[] = $value + $right->data()[$index];
        }

        return $result;
    }
}
