<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Graph;

use function is_bool;
use function is_scalar;
use function sprintf;

final class Trace
{
    /** @var list<Op> */
    private array $ops = [];

    public function add(Op $op): void
    {
        $this->ops[] = $op;
    }

    /** @return list<Op> */
    public function ops(): array
    {
        return $this->ops;
    }

    public function dump(): string
    {
        $lines = [];
        foreach ($this->ops as $op) {
            $inputs = implode(', ', array_map(
                static fn(int $id): string => '%' . $id,
                $op->inputIds(),
            ));
            $attrs = [];
            foreach ($op->attrs() as $key => $value) {
                if ($value instanceof Weight) {
                    $attrs[] = $key . '=' . $value->name();
                } elseif (is_bool($value)) {
                    $attrs[] = $key . '=' . ($value ? 'true' : 'false');
                } elseif (is_scalar($value)) {
                    $attrs[] = $key . '=' . $value;
                }
            }
            $suffix = $attrs === [] ? '' : ' ' . implode(' ', $attrs);
            $lines[] = sprintf(
                '%%%d = %s(%s)%s : %s %s',
                $op->outputId(),
                $op->name(),
                $inputs,
                $suffix,
                $op->outputShape(),
                $op->outputDtype(),
            );
        }

        return implode("\n", $lines) . ($lines === [] ? '' : "\n");
    }
}
