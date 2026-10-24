<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Graph\Kernel;

use Swow\Examples\Transformer\Graph\DType;
use Swow\Examples\Transformer\Graph\GraphException;
use Swow\Examples\Transformer\Graph\Kernel;
use Swow\Examples\Transformer\Graph\Weight;
use Swow\Examples\Transformer\Thread\Linear;

use function count;

final class LinearKernel implements Kernel
{
    public function execute(array $inputs, array $attrs): array
    {
        [$input] = $inputs;
        $weight = $attrs['weight'] ?? null;
        if (!$weight instanceof Weight) {
            throw new GraphException('linear requires a Weight');
        }
        if ($input->dtype() !== DType::F32) {
            throw new GraphException('linear input must be f32');
        }
        if (count($input->data()) !== $weight->columns()) {
            throw new GraphException('linear input width does not match weight columns');
        }
        if (($attrs['transpose'] ?? false) === true) {
            throw new GraphException('transposed linear is not implemented');
        }

        return Linear::matvec($weight, $input->data());
    }
}
