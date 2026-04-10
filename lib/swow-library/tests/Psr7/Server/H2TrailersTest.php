<?php

declare(strict_types=1);

namespace Swow\Tests\Psr7\Server;

use PHPUnit\Framework\Attributes\CoversClass;
use PHPUnit\Framework\TestCase;
use Swow\Psr7\Server\H2ResponseEnvelope;
use Swow\Psr7\Server\H2ServerConnection;
use Swow\Psr7\Server\H2Trailers;
use Swow\Psr7\Psr7;

#[CoversClass(H2Trailers::class)]
final class H2TrailersTest extends TestCase
{
    public function testTrailersObjectNormalizesScalarAndListValues(): void
    {
        $trailers = H2Trailers::fromArray([
            'x-a' => '1',
            'x-b' => ['2', '3'],
        ]);

        $this->assertSame(['1'], $trailers->get('x-a'));
        $this->assertSame(['2', '3'], $trailers->get('x-b'));
    }

    public function testResponseEnvelopeAcceptsTrailersObject(): void
    {
        $envelope = H2ResponseEnvelope::from(
            Psr7::createResponse(200),
            H2Trailers::fromArray(['x-a' => 'done'])
        );

        $this->assertSame(['x-a' => ['done']], $envelope->trailersObject()->toArray());
    }

    public function testRequestTrailersObjectReturnsNormalizedTrailers(): void
    {
        $request = Psr7::createServerRequest('POST', 'https://example.com/', [])
            ->withAttribute(H2ServerConnection::REQUEST_TRAILERS_ATTRIBUTE, ['x-a' => ['done']]);

        $this->assertSame(
            ['x-a' => ['done']],
            H2ServerConnection::requestTrailersObject($request)->toArray()
        );
    }
}
