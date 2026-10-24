<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Tests;

use PHPUnit\Framework\TestCase;
use RuntimeException;
use Swow\Examples\Transformer\Sampler\Sampler;
use Swow\Examples\Transformer\Sampler\SamplingConfig;
use Swow\Examples\Transformer\Sampler\SamplingContext;

use const INF;

/**
 * @internal
 */
final class SamplerTest extends TestCase
{
    public function testGreedy(): void
    {
        $config = new SamplingConfig(temperature: 0.0);
        $sampler = new Sampler($config);
        $this->assertSame(1, $sampler->sample([1.0, 3.0, 2.0]));
    }

    public function testTopKOneIsGreedy(): void
    {
        $config = new SamplingConfig(topK: 1, temperature: 1.0);
        $sampler = new Sampler($config);
        $this->assertSame(2, $sampler->sample([1.0, 2.0, 5.0]));
    }

    public function testTopKKeepsCandidates(): void
    {
        $config = new SamplingConfig(topK: 2, topP: 1.0, minP: 0.0, temperature: 1.0, seed: 7);
        $sampler = new Sampler($config);
        for ($iteration = 0; $iteration < 50; $iteration++) {
            $this->assertContains($sampler->sample([1.0, 4.0, 3.0, 0.0]), [1, 2]);
        }
    }

    public function testTopPAndMinP(): void
    {
        $config = new SamplingConfig(topK: 0, topP: 0.01, minP: 0.0, temperature: 1.0);
        $sampler = new Sampler($config);
        $this->assertSame(2, $sampler->sample([1.0, 2.0, 10.0]));

        $config = new SamplingConfig(topK: 0, topP: 1.0, minP: 1.0, temperature: 1.0);
        $sampler = new Sampler($config);
        $this->assertSame(1, $sampler->sample([4.0, 10.0, 5.0]));
    }

    public function testPenalty(): void
    {
        $config = new SamplingConfig(temperature: 0.0, repeatPenalty: 2.0, penaltyLastN: 4);
        $sampler = new Sampler($config);
        $context = new SamplingContext(history: [0]);
        $this->assertSame(1, $sampler->sample([10.0, 9.0], $context));
    }

    public function testSeedIsDeterministic(): void
    {
        $config = new SamplingConfig(topK: 0, topP: 1.0, minP: 0.0, temperature: 1.0, seed: 123);
        $first = new Sampler($config);
        $second = new Sampler($config);
        $logits = [0.5, 1.0, 1.5, 2.0];
        $this->assertSame(
            $first->sample($logits, new SamplingContext()),
            $second->sample($logits, new SamplingContext()),
        );
    }

    public function testAllFilteredThrows(): void
    {
        $this->expectException(RuntimeException::class);
        $sampler = new Sampler(new SamplingConfig());
        $sampler->sample([-INF, -INF]);
    }
}
