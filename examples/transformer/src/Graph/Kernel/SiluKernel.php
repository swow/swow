<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Graph\Kernel;

use Swow\Examples\Transformer\Graph\Kernel;

use function exp;

final class SiluKernel implements Kernel
{
    public function execute(array $inputs, array $attrs): array
    {
        [$input] = $inputs;
        $result = [];
        foreach ($input->data() as $value) {
            $result[] = $value / (1.0 + exp(-$value));
        }

        return $result;
    }
}
