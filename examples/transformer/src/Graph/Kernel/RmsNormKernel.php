<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Graph\Kernel;

use Swow\Examples\Transformer\Graph\DType;
use Swow\Examples\Transformer\Graph\GraphException;
use Swow\Examples\Transformer\Graph\Kernel;

use function count;
use function sqrt;

final class RmsNormKernel implements Kernel
{
    public function execute(array $inputs, array $attrs): array
    {
        [$input, $weight] = $inputs;
        $eps = (float) ($attrs['eps'] ?? 1e-6);
        if ($input->dtype() !== DType::F32 || $weight->dtype() !== DType::F32) {
            throw new GraphException('rms_norm requires f32 input and weight');
        }
        $data = $input->data();
        $weights = $weight->data();
        if (count($data) !== count($weights)) {
            throw new GraphException('rms_norm shape mismatch');
        }
        $square = 0.0;
        foreach ($data as $value) {
            $square += $value * $value;
        }
        $scale = 1.0 / sqrt($square / count($data) + $eps);
        $result = [];
        foreach ($data as $index => $value) {
            $result[] = $value * $scale * $weights[$index];
        }

        return $result;
    }
}
