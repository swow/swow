<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Tests;

use PHPUnit\Framework\TestCase;
use Swow\Examples\Transformer\Gguf\GgmlType;
use Swow\Examples\Transformer\Gguf\GgufArrayRef;
use Swow\Examples\Transformer\Gguf\GgufException;
use Swow\Examples\Transformer\Gguf\GgufFile;

use function count;
use function strlen;

/**
 * @internal
 */
final class GgufFileTest extends TestCase
{
    private const Q8_PATH = __DIR__ . '/../models/qwen3.5-0.8b/Qwen3.5-0.8B.Q8_0.gguf';

    private const F32_PATH = __DIR__ . '/../models/qwen3.5-0.8b/Qwen3.5-0.8B.F32.gguf';

    public function testQ8MetadataAndTensors(): void
    {
        if (!is_file(self::Q8_PATH)) {
            $this->markTestSkipped('Qwen3.5 Q8 model is not downloaded');
        }

        $file = GgufFile::open(self::Q8_PATH);
        try {
            $this->assertSame(3, $file->version());
            $this->assertSame(32, $file->alignment());
            $this->assertSame(320, count($file->tensors()));

            $metadata = $file->metadata();
            $this->assertSame('qwen35', $metadata->get('general.architecture'));
            $this->assertSame(24, $metadata->get('qwen35.block_count'));
            $this->assertSame(1024, $metadata->get('qwen35.embedding_length'));
            $this->assertSame(3584, $metadata->get('qwen35.feed_forward_length'));
            $this->assertSame([11, 11, 10, 0], $metadata->get('qwen35.rope.dimension_sections'));

            $tokens = $metadata->get('tokenizer.ggml.tokens');
            $this->assertInstanceOf(GgufArrayRef::class, $tokens);
            $this->assertSame(248320, $tokens->count());

            $embedding = $file->tensor('token_embd.weight');
            $this->assertSame([1024, 248320], $embedding->dimensions());
            $this->assertSame(GgmlType::Q8_0, $embedding->ggmlType());
            $this->assertSame(248320, $embedding->rows());
            $this->assertSame(1024, $embedding->columns());
            $this->assertSame(1088, $embedding->rowBytes());
            $this->assertSame(270172160, $embedding->byteSize());

            $row = $file->row('token_embd.weight', 0);
            $this->assertSame(1088, strlen($row));

            $ssmA = $file->tensor('blk.0.ssm_a');
            $this->assertSame(GgmlType::F32, $ssmA->ggmlType());
            $this->assertSame(64, strlen($file->readAll('blk.0.ssm_a')));

            $q = $file->tensor('blk.3.attn_q.weight');
            $this->assertSame([1024, 4096], $q->dimensions());
            $this->assertSame(2176, strlen($file->rows('blk.3.attn_q.weight', 0, 2)));

            $this->assertSame(
                $file->dataStart() + $embedding->relativeOffset(),
                $embedding->dataOffset(),
            );
        } finally {
            $file->close();
        }
    }

    public function testF32TensorRows(): void
    {
        if (!is_file(self::F32_PATH)) {
            $this->markTestSkipped('Qwen3.5 F32 model is not downloaded');
        }

        $file = GgufFile::open(self::F32_PATH);
        try {
            $embedding = $file->tensor('token_embd.weight');
            $this->assertSame(GgmlType::F32, $embedding->ggmlType());
            $this->assertSame(4096, $embedding->rowBytes());

            $row = $file->row('token_embd.weight', 0);
            $this->assertSame(4096, strlen($row));
            $this->assertIsFloat(unpack('g', substr($row, 0, 4))[1]);
        } finally {
            $file->close();
        }
    }

    public function testInvalidFileIsRejected(): void
    {
        $path = tempnam(sys_get_temp_dir(), 'gguf-test-');
        $this->assertNotFalse($path);
        file_put_contents($path, 'not a gguf file');

        try {
            $this->expectException(GgufException::class);
            GgufFile::open($path);
        } finally {
            @unlink($path);
        }
    }
}
