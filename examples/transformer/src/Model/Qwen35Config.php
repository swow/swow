<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Model;

use Swow\Examples\Transformer\Gguf\GgufMetadata;

final class Qwen35Config
{
    public int $hiddenSize;
    public int $blockCount;
    public int $ffnSize;
    public int $headCount;
    public int $headCountKv;
    public int $headDim;
    public int $fullAttentionInterval;
    public int $convKernel;
    public int $ssmStateSize;
    public int $ssmGroupCount;
    public int $ssmTimeStepRank;
    public int $ssmInnerSize;
    public float $rmsEps;
    public float $ropeBase;
    /** @var list<int> */
    public array $ropeSections;

    public static function fromMetadata(GgufMetadata $metadata): self
    {
        $config = new self();
        $config->hiddenSize = (int) $metadata->get('qwen35.embedding_length');
        $config->blockCount = (int) $metadata->get('qwen35.block_count');
        $config->ffnSize = (int) $metadata->get('qwen35.feed_forward_length');
        $config->headCount = (int) $metadata->get('qwen35.attention.head_count');
        $config->headCountKv = (int) $metadata->get('qwen35.attention.head_count_kv');
        $config->headDim = (int) $metadata->get('qwen35.attention.key_length');
        $config->fullAttentionInterval = (int) $metadata->get('qwen35.full_attention_interval');
        $config->convKernel = (int) $metadata->get('qwen35.ssm.conv_kernel');
        $config->ssmStateSize = (int) $metadata->get('qwen35.ssm.state_size');
        $config->ssmGroupCount = (int) $metadata->get('qwen35.ssm.group_count');
        $config->ssmTimeStepRank = (int) $metadata->get('qwen35.ssm.time_step_rank');
        $config->ssmInnerSize = (int) $metadata->get('qwen35.ssm.inner_size');
        $config->rmsEps = (float) $metadata->get('qwen35.attention.layer_norm_rms_epsilon');
        $config->ropeBase = (float) $metadata->get('qwen35.rope.freq_base');
        $config->ropeSections = $metadata->get('qwen35.rope.dimension_sections');

        return $config;
    }

    public function isFullAttention(int $layer): bool
    {
        return ($layer + 1) % $this->fullAttentionInterval === 0;
    }

    public function valueDim(): int
    {
        return intdiv($this->ssmInnerSize, $this->ssmTimeStepRank);
    }

    public function keyDim(): int
    {
        return $this->ssmStateSize;
    }
}
