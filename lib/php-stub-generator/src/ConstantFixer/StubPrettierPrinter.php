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

use PhpParser\Node\Stmt;
use PhpParser\PrettyPrinter\Standard as StandardPrinter;
use function count;

class StubPrettierPrinter extends StandardPrinter
{
    private bool $fileDocPadded = false;

    protected function pStmts(array $nodes, bool $indent = true): string
    {
        if ($indent) {
            $this->indent();
        }

        $beforeNode = null;
        $result = '';
        foreach ($nodes as $node) {
            if (
                $beforeNode &&
                $node instanceof Stmt\ClassMethod
            ) {
                $result .= "\n";
            }

            $comments = $node->getComments();
            if ($comments) {
                $result .= $this->nl . $this->pComments($comments);
                if ($node instanceof Stmt\Nop) {
                    continue;
                }
            }

            if (!$this->fileDocPadded) {
                $this->fileDocPadded = true;
                $result .= $this->nl;
            }

            $result .= $this->nl . $this->p($node);
            $beforeNode = $node;
        }

        if ($indent) {
            $this->outdent();
        }

        return $result;
    }

    protected function pClassCommon(Stmt\Class_ $node, mixed $afterClassToken): string
    {
        return $this->pAttrGroups($node->attrGroups, $node->name === null)
            . $this->pModifiers($node->flags)
            . 'class' . $afterClassToken
            . ($node->extends !== null ? ' extends ' . $this->p($node->extends) : '')
            . (!empty($node->implements) ? ' implements ' . $this->pCommaSeparated($node->implements) : '')
            . ((count($node->stmts) > 0) ? $this->nl . '{' . $this->pStmts($node->stmts) . $this->nl . '}' : ' { }');
    }

    protected function pStmt_ClassMethod(Stmt\ClassMethod $node): string
    {
        return $this->pAttrGroups($node->attrGroups)
            . $this->pModifiers($node->flags)
            . 'function ' . ($node->byRef ? '&' : '') . $node->name
            . '(' . $this->pMaybeMultiline($node->params) . ')'
            . ($node->returnType !== null ? ': ' . $this->p($node->returnType) : '')
            . ($node->stmts !== null
                ? ' { }'
                : ';');
    }

    protected function pModifiers(int $modifiers): string
    {
        return ($modifiers & Stmt\Class_::MODIFIER_FINAL ? 'final ' : '')
            . ($modifiers & Stmt\Class_::MODIFIER_PUBLIC ? 'public ' : '')
            . ($modifiers & Stmt\Class_::MODIFIER_PROTECTED ? 'protected ' : '')
            . ($modifiers & Stmt\Class_::MODIFIER_PRIVATE ? 'private ' : '')
            . ($modifiers & Stmt\Class_::MODIFIER_ABSTRACT ? 'abstract ' : '')
            . ($modifiers & Stmt\Class_::MODIFIER_STATIC ? 'static ' : '')
            . ($modifiers & Stmt\Class_::MODIFIER_READONLY ? 'readonly ' : '');
    }

    protected function pStmt_Namespace(Stmt\Namespace_ $node): string
    {
        if ($this->canUseSemicolonNamespaces) {
            return 'namespace ' . $this->p($node->name) . ';'
                . $this->nl . $this->pStmts($node->stmts, false);
        }
        return 'namespace' . ($node->name !== null ? ' ' . $this->p($node->name) : '')
                . $this->nl . '{' . $this->pStmts($node->stmts) . $this->nl . "}\n";
    }

    protected function pStmt_Function(Stmt\Function_ $node): string
    {
        return $this->pAttrGroups($node->attrGroups)
            . 'function ' . ($node->byRef ? '&' : '') . $node->name
            . '(' . $this->pCommaSeparated($node->params) . ')'
            . ($node->returnType !== null ? ': ' . $this->p($node->returnType) : '')
            . ' { }';
    }
}
