<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Tests;

use PHPUnit\Framework\TestCase;
use Swow\Examples\Transformer\Graph\DType;
use Swow\Examples\Transformer\Graph\Graph;
use Swow\Examples\Transformer\Graph\Shape;
use Swow\Examples\Transformer\Graph\Weight;

use function array_map;
use function array_slice;
use function count;
use function file;
use function hex2bin;
use function intdiv;
use function preg_split;
use function str_starts_with;
use function trim;

/**
 * @internal
 */
final class GraphReferenceTest extends TestCase
{
    private const VECTORS = __DIR__ . '/../../../../php-gemm-ref/vectors/gemm_vectors.txt';

    public function testF32MatvecAgainstCReference(): void
    {
        $fields = $this->section('matvec_f32');
        $rows = (int) $fields[0];
        $columns = (int) $fields[1];
        $offset = 2;
        $weights = $this->floats($fields, $offset, $rows * $columns);
        $offset += $rows * $columns;
        $input = $this->floats($fields, $offset, $columns);
        $offset += $columns;
        $expected = $this->floats($fields, $offset, $rows);

        $graph = new Graph();
        $x = $graph->input('x', new Shape($columns), $input);
        $y = $graph->linear($x, Weight::f32('matvec_f32', $rows, $columns, $weights));

        $this->assertClose($expected, $y->data());
        $this->assertSame('linear', $graph->trace()->ops()[0]->name());
        $this->assertSame([$rows], $y->shape()->dimensions());
        $this->assertStringContainsString('%2 = linear', $graph->dump());
    }

    public function testQ40MatvecAgainstCReference(): void
    {
        $this->assertQuantizedMatvec('matvec_q4_0', 'q4_0', Weight::Q4_0);
    }

    public function testQ80MatvecAgainstCReference(): void
    {
        $this->assertQuantizedMatvec('matvec_q8_0', 'q8_0', Weight::Q8_0);
    }

    public function testElementwiseAndGatedNorm(): void
    {
        $graph = new Graph();
        $x = $graph->input('x', new Shape(3), [1.0, -2.0, 3.0]);
        $one = $graph->input('one', new Shape(3), [1.0, 1.0, 1.0]);
        $norm = $graph->rmsNorm($x, $one, 0.0);
        $sum = $graph->add($x, $one);
        $product = $graph->mul($x, $x);
        $silu = $graph->silu($x);

        $this->assertClose([1 / sqrt(14 / 3), -2 / sqrt(14 / 3), 3 / sqrt(14 / 3)], $norm->data());
        $this->assertSame([2.0, -1.0, 4.0], $sum->data());
        $this->assertSame([1.0, 4.0, 9.0], $product->data());
        foreach ($x->data() as $index => $value) {
            $this->assertEqualsWithDelta($value / (1.0 + exp(-$value)), $silu->data()[$index], 1e-12);
        }
    }

    public function testSwiGluAndEmbedding(): void
    {
        $graph = new Graph();
        $gate = $graph->input('gate', new Shape(2), [0.0, 1.0]);
        $up = $graph->input('up', new Shape(2), [2.0, 3.0]);
        $out = $graph->swiGlu($gate, $up);

        $this->assertSame(0.0, $out->data()[0]);
        $this->assertEqualsWithDelta(3.0 / (1.0 + exp(-1.0)), $out->data()[1], 1e-12);
        $this->assertCount(2, $graph->trace()->ops());
        $this->assertSame(['silu', 'mul'], array_map(
            static fn($op): string => $op->name(),
            $graph->trace()->ops(),
        ));

        $embeddingGraph = new Graph();
        $tokens = $embeddingGraph->input('tokens', new Shape(1), [1], DType::I32);
        $embedding = $embeddingGraph->embedding(
            Weight::f32('emb', 2, 3, [0.0, 0.0, 0.0, 1.0, 2.0, 3.0]),
            $tokens,
        );
        $this->assertSame([1.0, 2.0, 3.0], $embedding->data());
    }

    private function assertQuantizedMatvec(string $section, string $type, string $weightType): void
    {
        $fields = $this->section($section);
        $rows = (int) $fields[0];
        $columns = (int) $fields[1];
        $blocks = intdiv($columns, 32);
        $offset = 2;
        $packed = '';
        for ($index = 0; $index < $rows * $blocks; $index++) {
            $block = hex2bin($fields[$offset + $index]);
            $this->assertNotFalse($block);
            $packed .= $block;
        }
        $offset += $rows * $blocks;
        $input = $this->floats($fields, $offset, $columns);
        $offset += $columns;
        $expected = $this->floats($fields, $offset, $rows);

        $graph = new Graph();
        $x = $graph->input('x', new Shape($columns), $input);
        $y = $graph->linear($x, Weight::packed($section, $weightType, $rows, $columns, $packed));

        $this->assertClose($expected, $y->data());
        $this->assertSame([$rows], $y->shape()->dimensions());
    }

    private function section(string $name): array
    {
        $lines = file(self::VECTORS);
        $this->assertNotFalse($lines);
        $active = false;
        foreach ($lines as $line) {
            $line = trim($line);
            if ($line === '[' . $name . ']') {
                $active = true;
                continue;
            }
            if (!$active || $line === '' || str_starts_with($line, 'count ')) {
                continue;
            }
            return preg_split('/\s+/', $line) ?: [];
        }

        $this->fail("Missing vector section: {$name}");
    }

    /** @param list<string> $fields @return list<float> */
    private function floats(array $fields, int $offset, int $count): array
    {
        return array_map(
            static fn(string $value): float => (float) $value,
            array_slice($fields, $offset, $count),
        );
    }

    /** @param list<float> $expected @param list<float> $actual */
    private function assertClose(array $expected, array $actual): void
    {
        $this->assertCount(count($expected), $actual);
        foreach ($expected as $index => $value) {
            $this->assertEqualsWithDelta($value, $actual[$index], max(1e-5, abs($value) * 1e-4));
        }
    }
}
