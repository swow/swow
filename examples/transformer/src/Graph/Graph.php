<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Graph;

use Swow\Examples\Transformer\Graph\Kernel\AddKernel;
use Swow\Examples\Transformer\Graph\Kernel\EmbeddingKernel;
use Swow\Examples\Transformer\Graph\Kernel\LinearKernel;
use Swow\Examples\Transformer\Graph\Kernel\MulKernel;
use Swow\Examples\Transformer\Graph\Kernel\RmsNormKernel;
use Swow\Examples\Transformer\Graph\Kernel\SiluKernel;

use function count;

final class Graph
{
    private Trace $trace;

    private KernelRegistry $kernels;

    private int $nextId = 0;

    /** @var list<Tensor> */
    private array $inputs = [];

    private ?Tensor $output = null;

    public function __construct()
    {
        $this->trace = new Trace();
        $this->kernels = new KernelRegistry();
        $this->kernels->register('linear', new LinearKernel());
        $this->kernels->register('rms_norm', new RmsNormKernel());
        $this->kernels->register('silu', new SiluKernel());
        $this->kernels->register('add', new AddKernel());
        $this->kernels->register('mul', new MulKernel());
        $this->kernels->register('embedding', new EmbeddingKernel());
    }

    public function registerKernel(string $name, Kernel $kernel): void
    {
        $this->kernels->register($name, $kernel);
    }

    /** @param list<float>|list<int> $data */
    public function input(string $name, Shape $shape, array $data, string $dtype = DType::F32): Tensor
    {
        $tensor = new Tensor(++$this->nextId, $name, $shape, $dtype, $data);
        $this->inputs[] = $tensor;

        return $tensor;
    }

    /** @param list<float>|list<int> $data */
    public function value(string $name, Shape $shape, array $data, string $dtype = DType::F32): Tensor
    {
        return new Tensor(++$this->nextId, $name, $shape, $dtype, $data);
    }

    public function linear(Tensor $input, Weight $weight, bool $transpose = false): Tensor
    {
        if (count($input->shape()->dimensions()) !== 1) {
            throw new GraphException('linear requires a 1D input');
        }
        if (($input->shape()->dimensions()[0] ?? 0) !== $weight->columns()) {
            throw new GraphException('linear input width does not match weight columns');
        }

        return $this->apply(
            'linear',
            [$input],
            new Shape($weight->rows()),
            DType::F32,
            ['weight' => $weight, 'transpose' => $transpose],
        );
    }

    public function rmsNorm(Tensor $input, Tensor $weight, float $eps = 1e-6): Tensor
    {
        if (!$input->shape()->equals($weight->shape())) {
            throw new GraphException('rms_norm shape mismatch');
        }

        return $this->apply('rms_norm', [$input, $weight], $input->shape(), DType::F32, ['eps' => $eps]);
    }

    public function add(Tensor $left, Tensor $right): Tensor
    {
        if (!$left->shape()->equals($right->shape()) || $left->dtype() !== $right->dtype()) {
            throw new GraphException('add requires matching shapes and dtypes');
        }

        return $this->apply('add', [$left, $right], $left->shape(), $left->dtype(), []);
    }

    public function mul(Tensor $left, Tensor $right): Tensor
    {
        if (!$left->shape()->equals($right->shape()) || $left->dtype() !== $right->dtype()) {
            throw new GraphException('mul requires matching shapes and dtypes');
        }

        return $this->apply('mul', [$left, $right], $left->shape(), $left->dtype(), []);
    }

    public function silu(Tensor $input): Tensor
    {
        return $this->apply('silu', [$input], $input->shape(), DType::F32, []);
    }

    public function swiGlu(Tensor $gate, Tensor $up): Tensor
    {
        return $this->mul($this->silu($gate), $up);
    }

    public function embedding(Weight $weight, Tensor $tokens): Tensor
    {
        if ($tokens->dtype() !== DType::I32 || count($tokens->data()) !== 1) {
            throw new GraphException('embedding requires a single i32 token id');
        }

        return $this->apply(
            'embedding',
            [$tokens],
            new Shape($weight->columns()),
            DType::F32,
            ['weight' => $weight],
        );
    }

    public function output(Tensor $tensor): void
    {
        $this->output = $tensor;
    }

    public function trace(): Trace
    {
        return $this->trace;
    }

    /** @return list<Tensor> */
    public function inputs(): array
    {
        return $this->inputs;
    }

    public function outputTensor(): ?Tensor
    {
        return $this->output;
    }

    public function dump(): string
    {
        return $this->trace->dump();
    }

    private function apply(
        string $name,
        array $inputs,
        Shape $outputShape,
        string $outputDtype,
        array $attrs,
    ): Tensor {
        $kernel = $this->kernels->get($name);
        $data = $kernel->execute($inputs, $attrs);
        $tensor = new Tensor(++$this->nextId, $name . '_' . $this->nextId, $outputShape, $outputDtype, $data);
        $this->trace->add(new Op(
            count($this->trace->ops()),
            $name,
            array_map(static fn(Tensor $input): int => $input->id(), $inputs),
            $tensor->id(),
            array_map(static fn(Tensor $input): Shape => $input->shape(), $inputs),
            array_map(static fn(Tensor $input): string => $input->dtype(), $inputs),
            $tensor->shape(),
            $tensor->dtype(),
            $attrs,
        ));

        return $tensor;
    }
}
