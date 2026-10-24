<?php

declare(strict_types=1);

namespace Swow\Examples\Transformer\Tests;

use PHPUnit\Framework\TestCase;
use Swow\Examples\Transformer\Reference\Logits;

use function file_put_contents;
use function pack;
use function tempnam;
use function unlink;

/**
 * @internal
 */
final class LogitsTest extends TestCase
{
    public function testReadAndCompare(): void
    {
        $referencePath = tempnam(sys_get_temp_dir(), 'logits-ref-');
        $actualPath = tempnam(sys_get_temp_dir(), 'logits-actual-');
        $this->assertNotFalse($referencePath);
        $this->assertNotFalse($actualPath);

        try {
            file_put_contents($referencePath, pack('g*', 1.0, 3.0, 2.0, 0.0));
            file_put_contents($actualPath, pack('g*', 1.0, 3.0, 2.0, 0.5));

            $result = Logits::compare(Logits::read($referencePath), Logits::read($actualPath), 3);
            $this->assertSame(4, $result['count']);
            $this->assertSame(0.5, $result['max_abs']);
            $this->assertSame([1, 2, 0], $result['top_reference']);
            $this->assertSame([1, 2, 0], $result['top_actual']);
            $this->assertTrue($result['top1_match']);
        } finally {
            @unlink($referencePath);
            @unlink($actualPath);
        }
    }
}
