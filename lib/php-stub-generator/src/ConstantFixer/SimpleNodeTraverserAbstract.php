<?php

/**
 * This file is part of Swow
 *
 * @link    https://github.com/swow/swow
 * @contact twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

namespace Swow\StubUtils\ConstantFixer;

use PhpParser\Node;
use PhpParser\NodeVisitorAbstract;

abstract class SimpleNodeTraverserAbstract extends NodeVisitorAbstract
{
    protected ?string $namespace = null;
    protected ?string $class = null;

    public function enterNode(Node $node)
    {
        switch (true) {
            case $node instanceof Node\Stmt\Namespace_:
                // entering namespace
                if ($node->name && $node->name->parts) {
                    $this->namespace = implode('\\', $node->name->parts);
                } else {
                    $this->namespace = '!GLOBAL!';
                }
                break;
            case $node instanceof Node\Stmt\Class_:
                // stub donot have a inlined class
                $this->class = $node->name->name;
                break;
        }
        return null;
    }

    public function leaveNode(Node $node)
    {
        switch (true) {
            case $node instanceof Node\Stmt\Namespace_:
                $this->namespace = null;
                break;
            case $node instanceof Node\Stmt\Class_:
                $this->class = null;
                break;
        }
        return null;
    }

    protected function inNamespace(string $namespace): bool
    {
        return $this->namespace === $namespace;
    }

    protected function inClass(string $class): bool
    {
        return $this->class === $class;
    }

    protected function inNamespaceClass(string $namespace, string $class): bool
    {
        return $this->namespace === $namespace && $this->class === $class;
    }
}
