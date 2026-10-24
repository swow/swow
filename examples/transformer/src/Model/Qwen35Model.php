<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Model;

use RuntimeException;
use Swow\Examples\Gemm\Quantization\Q4_0;
use Swow\Examples\Gemm\Quantization\Q8_0;
use Swow\Examples\Transformer\Gguf\GgmlType;
use Swow\Examples\Transformer\Gguf\GgufFile;
use Swow\Examples\Transformer\Graph\Graph;
use Swow\Examples\Transformer\Graph\Shape;
use Swow\Examples\Transformer\Graph\Weight;
use Swow\Examples\Transformer\Thread\CooperativeContext;
use Swow\Examples\Transformer\Thread\Linear;

use function array_fill;
use function array_slice;
use function array_values;
use function count;
use function exp;
use function intdiv;
use function log1p;
use function max;
use function sin;
use function sqrt;
use function unpack;

final class Qwen35Model
{
    private Qwen35Config $config;

    private ?Weight $tokenEmbedding = null;

    /** @var array<string, Weight> */
    private array $weights = [];

    /** @var array<int, array<int, list<float>>> */
    private array $kCache = [];

    /** @var array<int, array<int, list<float>>> */
    private array $vCache = [];

    /** @var array<int, list<float>> */
    private array $convState = [];

    /** @var array<int, list<float>> */
    private array $ssmState = [];

    /** @var array<int, float> */
    private array $ropeInverse = [];

    /** @var array<string, list<float>> */
    private array $f32Cache = [];

    private Graph $graph;

    /** @var callable|null */
    private $debug;

    public function __construct(private GgufFile $file)
    {
        $this->config = Qwen35Config::fromMetadata($file->metadata());
        $this->graph = new Graph();
    }

    public function config(): Qwen35Config
    {
        return $this->config;
    }

    public function graph(): Graph
    {
        return $this->graph;
    }

    public function reset(): void
    {
        $this->kCache = [];
        $this->vCache = [];
        $this->convState = [];
        $this->ssmState = [];
        $this->graph = new Graph();
    }

    /** @param callable(string, list<float>): void $debug */
    public function setDebug(callable $debug): void
    {
        $this->debug = $debug;
    }

    /** @return list<float> */
    public function forward(int $token, int $position): array
    {
        $this->graph = new Graph();
        $embedding = $this->embeddingRow($token);
        $this->debug('model.input_embed', $embedding);
        $x = $this->graph->value('embedding', new Shape($this->config->hiddenSize), $embedding);

        for ($layer = 0; $layer < $this->config->blockCount; $layer++) {
            $normData = $this->rmsNorm(
                $x,
                $this->f32Tensor('blk.' . $layer . '.attn_norm.weight'),
                $this->config->rmsEps,
            )->data();
            $this->debug('attn_norm-' . $layer, $normData);
            $norm = $this->graph->value('attn_norm_' . $layer, new Shape($this->config->hiddenSize), $normData);

            $attention = $this->config->isFullAttention($layer)
                ? $this->fullAttention($norm, $layer, $position)
                : $this->gatedDeltaNet($norm, $layer, $position);
            $this->debug(
                ($this->config->isFullAttention($layer) ? 'attn_output-' : 'linear_attn_out-') . $layer,
                $attention,
            );

            $x = $this->graph->add(
                $x,
                $this->graph->value('attn_out_' . $layer, new Shape($this->config->hiddenSize), $attention),
            );
            $this->debug('attn_residual-' . $layer, $x->data());

            $postNorm = $this->rmsNorm(
                $x,
                $this->f32Tensor('blk.' . $layer . '.post_attention_norm.weight'),
                $this->config->rmsEps,
            );
            $this->debug('attn_post_norm-' . $layer, $postNorm->data());
            $ffnInput = $postNorm;
            $x = $this->layerFfn($x, $ffnInput, $layer);
            $this->debug('l_out-' . $layer, $x->data());
        }

        $norm = $this->rmsNorm(
            $x,
            $this->f32Tensor('output_norm.weight'),
            $this->config->rmsEps,
        );
        $this->debug('result_norm', $norm->data());
        $logits = $this->graph->linear($norm, $this->tokenEmbedding());
        $this->debug('result_output', $logits->data());

        return $logits->data();
    }

    /**
     * @param list<Weight> $weights
     * @return list<\Swow\Examples\Transformer\Graph\Tensor>
     */
    private function linearMany(\Swow\Examples\Transformer\Graph\Tensor $input, array $weights): array
    {
        $results = Linear::matvecMany($weights, $input->data());
        $tensors = [];
        foreach ($weights as $index => $weight) {
            $tensors[] = $this->graph->value(
                'linear_batch_' . $weight->name(),
                new Shape($weight->rows()),
                $results[$index],
            );
        }

        return $tensors;
    }

    private function layerFfn(\Swow\Examples\Transformer\Graph\Tensor $residual, \Swow\Examples\Transformer\Graph\Tensor $input, int $layer): \Swow\Examples\Transformer\Graph\Tensor
    {
        $prefix = 'blk.' . $layer;
        [$gate, $up] = $this->linearMany($input, [
            $this->weight($prefix . '.ffn_gate.weight'),
            $this->weight($prefix . '.ffn_up.weight'),
        ]);
        $swiGlu = $this->graph->swiGlu($gate, $up);
        $ffn = $this->graph->linear(
            $swiGlu,
            $this->weight($prefix . '.ffn_down.weight'),
        );
        $this->debug('ffn_gate-' . $layer, $gate->data());
        $this->debug('ffn_up-' . $layer, $up->data());
        $this->debug('ffn_swiglu-' . $layer, $swiGlu->data());
        $this->debug('ffn_out-' . $layer, $ffn->data());

        return $this->graph->add($residual, $ffn);
    }

    private function rmsNorm(\Swow\Examples\Transformer\Graph\Tensor $input, array $weight, float $eps): \Swow\Examples\Transformer\Graph\Tensor
    {
        $w = $this->graph->value('norm_weight', new Shape(count($weight)), $weight);

        return $this->graph->rmsNorm($input, $w, $eps);
    }

    /** @return list<float> */
    private function embeddingRow(int $token): array
    {
        $info = $this->file->tensor('token_embd.weight');
        if ($token < 0 || $token >= $info->rows()) {
            throw new RuntimeException('Token id out of range');
        }
        $row = $this->file->row('token_embd.weight', $token);

        return match ($info->ggmlType()) {
            GgmlType::F32 => array_values(unpack('g*', $row)),
            GgmlType::Q8_0 => Q8_0::dequantize($row),
            GgmlType::Q4_0 => Q4_0::dequantize($row),
            default => throw new RuntimeException('Unsupported embedding type'),
        };
    }

    private function weight(string $name): Weight
    {
        if (!isset($this->weights[$name])) {
            $this->weights[$name] = CooperativeContext::active()
                ? Weight::fromGgufLazy($this->file, $name)
                : Weight::fromGguf($this->file, $name);
        }

        return $this->weights[$name];
    }

    private function tokenEmbedding(): Weight
    {
        return $this->weight('token_embd.weight');
    }

    /** @return list<float> */
    private function f32Tensor(string $name): array
    {
        if (isset($this->f32Cache[$name])) {
            return $this->f32Cache[$name];
        }
        $raw = $this->file->readAll($name);
        $info = $this->file->tensor($name);
        if ($info->ggmlType() !== GgmlType::F32) {
            throw new RuntimeException("Expected F32 tensor: {$name}");
        }

        return $this->f32Cache[$name] = array_values(unpack('g*', $raw));
    }

    /** @return list<float> */
    private function fullAttention(\Swow\Examples\Transformer\Graph\Tensor $input, int $layer, int $position): array
    {
        $prefix = 'blk.' . $layer;
        [$qgTensor, $kTensor, $vTensor] = $this->linearMany($input, [
            $this->weight($prefix . '.attn_q.weight'),
            $this->weight($prefix . '.attn_k.weight'),
            $this->weight($prefix . '.attn_v.weight'),
        ]);
        $qg = $qgTensor->data();
        $k = $kTensor->data();
        $v = $vTensor->data();
        $this->debug('Qcur_full-' . $layer, $qg);
        $this->debug('Vcur-' . $layer, $v);

        $q = [];
        $gate = [];
        $headDim = $this->config->headDim;
        for ($head = 0; $head < $this->config->headCount; $head++) {
            $base = $head * 2 * $headDim;
            $q = [...$q, ...array_slice($qg, $base, $headDim)];
            $gate = [...$gate, ...array_slice($qg, $base + $headDim, $headDim)];
        }

        $qNorm = $this->f32Tensor($prefix . '.attn_q_norm.weight');
        $kNorm = $this->f32Tensor($prefix . '.attn_k_norm.weight');
        for ($head = 0; $head < $this->config->headCount; $head++) {
            $part = $this->rmsNormVector(array_slice($q, $head * $headDim, $headDim), $qNorm, $this->config->rmsEps);
            array_splice($q, $head * $headDim, $headDim, $part);
        }
        for ($head = 0; $head < $this->config->headCountKv; $head++) {
            $part = $this->rmsNormVector(array_slice($k, $head * $headDim, $headDim), $kNorm, $this->config->rmsEps);
            array_splice($k, $head * $headDim, $headDim, $part);
        }

        $this->debug('Qcur_normed-' . $layer, $q);
        $this->debug('Kcur_normed-' . $layer, $k);

        $this->applyRope($q, $this->config->headCount, $position);
        $this->applyRope($k, $this->config->headCountKv, $position);
        $this->debug('Qcur-' . $layer, $q);
        $this->debug('Kcur-' . $layer, $k);

        $this->kCache[$layer][$position] = $k;
        $this->vCache[$layer][$position] = $v;

        $headsPerKv = intdiv($this->config->headCount, $this->config->headCountKv);
        $scale = 1.0 / sqrt((float) $headDim);
        $attention = [];
        $pregate = [];
        for ($head = 0; $head < $this->config->headCount; $head++) {
            $kvHead = intdiv($head, $headsPerKv);
            $scores = [];
            $maximum = -1.0e30;
            for ($step = 0; $step <= $position; $step++) {
                $score = 0.0;
                $key = $this->kCache[$layer][$step];
                for ($dim = 0; $dim < $headDim; $dim++) {
                    $score += $q[$head * $headDim + $dim] * $key[$kvHead * $headDim + $dim];
                }
                $score *= $scale;
                $scores[$step] = $score;
                $maximum = max($maximum, $score);
            }
            $sum = 0.0;
            foreach ($scores as $step => $score) {
                $scores[$step] = exp($score - $maximum);
                $sum += $scores[$step];
            }
            for ($dim = 0; $dim < $headDim; $dim++) {
                $value = 0.0;
                for ($step = 0; $step <= $position; $step++) {
                    $value += $scores[$step] / $sum * $this->vCache[$layer][$step][$kvHead * $headDim + $dim];
                }
                $pregate[$head * $headDim + $dim] = $value;
                $attention[$head * $headDim + $dim] = $value * $this->sigmoid($gate[$head * $headDim + $dim]);
            }
        }
        $this->debug('attn_pregate-' . $layer, $pregate);
        $this->debug('attn_gated-' . $layer, $attention);

        $output = $this->graph->linear(
            $this->graph->value('attn_gated_' . $layer, new Shape($this->config->headCount * $headDim), $attention),
            $this->weight($prefix . '.attn_output.weight'),
        )->data();
        $this->debug('attn_output-' . $layer, $output);

        return $output;
    }

    /** @return list<float> */
    private function gatedDeltaNet(\Swow\Examples\Transformer\Graph\Tensor $input, int $layer, int $position): array
    {
        $prefix = 'blk.' . $layer;
        [$qkvTensor, $zTensor, $betaTensor, $alphaTensor] = $this->linearMany($input, [
            $this->weight($prefix . '.attn_qkv.weight'),
            $this->weight($prefix . '.attn_gate.weight'),
            $this->weight($prefix . '.ssm_beta.weight'),
            $this->weight($prefix . '.ssm_alpha.weight'),
        ]);
        $qkv = $qkvTensor->data();
        $z = $zTensor->data();
        $beta = $betaTensor->data();
        $alpha = $alphaTensor->data();

        $dt = $this->f32Tensor($prefix . '.ssm_dt.bias');
        $a = $this->f32Tensor($prefix . '.ssm_a');
        $g = [];
        for ($head = 0; $head < $this->config->ssmTimeStepRank; $head++) {
            $beta[$head] = $this->sigmoid($beta[$head]);
            $alpha[$head] = $this->softplus($alpha[$head] + $dt[$head]);
            $g[$head] = $alpha[$head] * $a[$head];
        }
        $this->debug('a_softplus-' . $layer, $alpha);
        $this->debug('gate-' . $layer, $g);
        $this->debug('beta_sigmoid-' . $layer, $beta);
        $this->debug('z-' . $layer, $z);

        $convWeight = $this->f32Tensor($prefix . '.ssm_conv1d.weight');
        $convDim = $this->config->ssmInnerSize + 2 * $this->config->ssmGroupCount * $this->config->ssmStateSize;
        if (!isset($this->convState[$layer])) {
            $this->convState[$layer] = array_fill(0, ($this->config->convKernel - 1) * $convDim, 0.0);
        }
        $history = $this->convState[$layer];
        $conv = [];
        for ($channel = 0; $channel < $convDim; $channel++) {
            $sum = 0.0;
            for ($kernel = 0; $kernel < $this->config->convKernel; $kernel++) {
                $value = $kernel === $this->config->convKernel - 1
                    ? $qkv[$channel]
                    : $history[$kernel * $convDim + $channel];
                $sum += $value * $convWeight[$channel * $this->config->convKernel + $kernel];
            }
            $conv[] = $this->silu($sum);
        }
        $this->debug('conv_output_silu-' . $layer, $conv);
        $this->convState[$layer] = array_merge(
            array_slice($history, $convDim),
            $qkv,
        );

        $keyDim = $this->config->keyDim();
        $valueDim = $this->config->valueDim();
        $q = array_slice($conv, 0, $this->config->ssmGroupCount * $keyDim);
        $k = array_slice($conv, $this->config->ssmGroupCount * $keyDim, $this->config->ssmGroupCount * $keyDim);
        $v = array_slice($conv, 2 * $this->config->ssmGroupCount * $keyDim, $this->config->ssmTimeStepRank * $valueDim);

        for ($head = 0; $head < $this->config->ssmGroupCount; $head++) {
            $part = array_slice($q, $head * $keyDim, $keyDim);
            $this->l2Norm($part);
            array_splice($q, $head * $keyDim, $keyDim, $part);
            $part = array_slice($k, $head * $keyDim, $keyDim);
            $this->l2Norm($part);
            array_splice($k, $head * $keyDim, $keyDim, $part);
        }
        $this->debug('q_conv_predelta-' . $layer, $q);
        $this->debug('k_conv_predelta-' . $layer, $k);
        $scale = 1.0 / sqrt((float) $keyDim);
        for ($index = 0; $index < count($q); $index++) {
            $q[$index] *= $scale;
        }

        if (!isset($this->ssmState[$layer])) {
            $this->ssmState[$layer] = array_fill(
                0,
                $this->config->ssmTimeStepRank * $valueDim * $valueDim,
                0.0,
            );
        }
        $state = $this->ssmState[$layer];
        $output = [];
        for ($head = 0; $head < $this->config->ssmTimeStepRank; $head++) {
            $stateOffset = $head * $valueDim * $valueDim;
            $qh = array_slice($q, $head * $keyDim, $keyDim);
            $kh = array_slice($k, $head * $keyDim, $keyDim);
            $vh = array_slice($v, $head * $valueDim, $valueDim);
            $decay = exp($g[$head]);
            $betaHead = $beta[$head];
            $delta = [];
            for ($index = 0; $index < $valueDim; $index++) {
                for ($column = 0; $column < $valueDim; $column++) {
                    $state[$stateOffset + $index + $valueDim * $column] *= $decay;
                }
            }
            for ($column = 0; $column < $valueDim; $column++) {
                $sum = 0.0;
                for ($index = 0; $index < $valueDim; $index++) {
                    $sum += $state[$stateOffset + $index + $valueDim * $column] * $kh[$index];
                }
                $delta[$column] = $betaHead * ($vh[$column] - $sum);
            }
            for ($index = 0; $index < $valueDim; $index++) {
                $ki = $kh[$index];
                for ($column = 0; $column < $valueDim; $column++) {
                    $state[$stateOffset + $index + $valueDim * $column] += $ki * $delta[$column];
                }
            }
            for ($column = 0; $column < $valueDim; $column++) {
                $sum = 0.0;
                for ($index = 0; $index < $valueDim; $index++) {
                    $sum += $state[$stateOffset + $index + $valueDim * $column] * $qh[$index];
                }
                $output[$head * $valueDim + $column] = $sum;
            }
        }
        $this->ssmState[$layer] = $state;

        $normWeight = $this->f32Tensor($prefix . '.ssm_norm.weight');
        for ($head = 0; $head < $this->config->ssmTimeStepRank; $head++) {
            $part = array_slice($output, $head * $valueDim, $valueDim);
            $part = $this->rmsNormVector($part, $normWeight, $this->config->rmsEps);
            for ($index = 0; $index < $valueDim; $index++) {
                $part[$index] *= $this->silu($z[$head * $valueDim + $index]);
                $output[$head * $valueDim + $index] = $part[$index];
            }
        }

        $this->debug('final_output-' . $layer, $output);
        $result = $this->graph->linear(
            $this->graph->value('gdn_out_' . $layer, new Shape($this->config->ssmInnerSize), $output),
            $this->weight($prefix . '.ssm_out.weight'),
        )->data();
        $this->debug('linear_attn_out-' . $layer, $result);

        return $result;
    }

    /** @param list<float> $values */
    private function applyRope(array &$values, int $heads, int $position): void
    {
        $dim = $this->config->headDim;
        if ($this->ropeInverse === []) {
            for ($index = 0; $index < 32; $index++) {
                $this->ropeInverse[$index] = $this->config->ropeBase ** (-(float) (2 * $index) / 64.0);
            }
        }
        for ($head = 0; $head < $heads; $head++) {
            $base = $head * $dim;
            for ($index = 0; $index < 32; $index++) {
                $angle = $position * $this->ropeInverse[$index];
                $cos = cos($angle);
                $sin = sin($angle);
                $a = $values[$base + $index];
                $b = $values[$base + $index + 32];
                $values[$base + $index] = $a * $cos - $b * $sin;
                $values[$base + $index + 32] = $b * $cos + $a * $sin;
            }
        }
    }

    /** @param list<float> $values @param list<float> $weight @return list<float> */
    private function rmsNormVector(array $values, array $weight, float $eps): array
    {
        $square = 0.0;
        foreach ($values as $value) {
            $square += $value * $value;
        }
        $scale = 1.0 / sqrt($square / count($values) + $eps);
        $result = [];
        foreach ($values as $index => $value) {
            $result[] = $value * $scale * $weight[$index];
        }

        return $result;
    }

    /** @param list<float> $values */
    private function l2Norm(array &$values): void
    {
        $square = 0.0;
        foreach ($values as $value) {
            $square += $value * $value;
        }
        $scale = 1.0 / sqrt($square + $this->config->rmsEps);
        foreach ($values as $index => $value) {
            $values[$index] = $value * $scale;
        }
    }

    /** @param list<float> $data */
    private function debug(string $name, array $data): void
    {
        if ($this->debug !== null) {
            ($this->debug)($name, $data);
        }
    }

    private function sigmoid(float $value): float
    {
        return 1.0 / (1.0 + exp(-$value));
    }

    private function softplus(float $value): float
    {
        return $value > 20.0 ? $value : log1p(exp($value));
    }

    private function silu(float $value): float
    {
        return $value * $this->sigmoid($value);
    }
}
