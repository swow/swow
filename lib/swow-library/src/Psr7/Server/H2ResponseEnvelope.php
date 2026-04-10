<?php

declare(strict_types=1);

namespace Swow\Psr7\Server;

use Psr\Http\Message\ResponseInterface;

final class H2ResponseEnvelope
{
    public function __construct(
        public ResponseInterface $response,
        private H2Trailers $trailers,
    ) {}

    /**
     * @param array<string, array<string>|string>|H2Trailers $trailers
     */
    public static function from(ResponseInterface $response, array|H2Trailers $trailers = []): self
    {
        return new self(
            $response,
            $trailers instanceof H2Trailers ? $trailers : H2Trailers::fromArray($trailers)
        );
    }

    /**
     * @return array<string, list<string>>
     */
    public function trailersMap(): array
    {
        return $this->trailers->toArray();
    }

    public function trailersObject(): H2Trailers
    {
        return $this->trailers;
    }
}
