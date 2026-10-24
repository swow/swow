<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Tests;

use PHPUnit\Framework\TestCase;
use Swow\Examples\Transformer\Gguf\GgufFile;
use Swow\Examples\Transformer\Model\Qwen35Model;
use Swow\Examples\Transformer\Reference\Logits;

use function getenv;
use function is_file;

/**
 * @internal
 */
final class Qwen35ModelTest extends TestCase
{
    private const MODEL = __DIR__ . '/../models/qwen3.5-0.8b/Qwen3.5-0.8B.F32.gguf';

    private const LLAMA_LOGITS = __DIR__ . '/../../../../build/qwen-llama/llama_logits_00.bin';

    public function testF32LogitsAgainstLlamaCpp(): void
    {
        if (getenv('RUN_QWEN35_SLOW') !== '1') {
            $this->markTestSkipped('Set RUN_QWEN35_SLOW=1 to run the full model');
        }
        if (!is_file(self::MODEL) || !is_file(self::LLAMA_LOGITS)) {
            $this->markTestSkipped('Qwen3.5 F32 model or llama.cpp logits are not available');
        }

        $file = GgufFile::open(self::MODEL);
        try {
            $model = new Qwen35Model($file);
            $logits = $model->forward(100, 0);
            $expected = Logits::read(self::LLAMA_LOGITS);
            $result = Logits::compare($expected, $logits);

            $this->assertTrue($result['top1_match']);
            $this->assertTrue($result['top5_match']);
            $this->assertLessThan(1e-4, $result['max_abs']);
        } finally {
            $file->close();
        }
    }
}
