#!/usr/bin/env php
<?php
/**
 * This file is part of Swow
 * OS-arch-dependent constant fixer
 *
 * @link     https://github.com/swow/swow
 * @contact  twosee <twosee@php.net>
 *
 * For the full copyright and license information,
 * please view the LICENSE file that was distributed with this source code
 */

declare(strict_types=1);

namespace Swow\StubUtils\ConstantFixer;

use PhpParser\Comment\Doc;
use PhpParser\Node;
use PhpParser\NodeTraverser;
use PhpParser\ParserFactory;

require __DIR__ . '/autoload.php';

$fixer = new ConstantFixer(
    modifiers: [
        // swow specified constant modifier
        function (
            string $content,
            array $constantDefinitions,
        ): string {
            $parser = (new ParserFactory())->create(ParserFactory::PREFER_PHP7);
            $ast = $parser->parse($content);
            $traverser = new NodeTraverser();
            $traverser->addVisitor(new class($constantDefinitions) extends SimpleNodeTraverserAbstract {
                /**
                 * @param array<string, ConstantDefinition> $constantDefinitions
                 */
                public function __construct(
                    private array $constantDefinitions,
                ) {
                }

                public function enterNode(Node $node)
                {
                    parent::enterNode($node);
                    if (
                        $node instanceof Node\Stmt\Class_ &&
                        $this->inNamespaceClass('Swow', 'Signal')
                    ) {
                        // entering class Swow\Signal
                        /** @var array<int, array{'value': int, 'name': string, 'stmt': array<mixed>}> */
                        $newStmts = [];
                        $signalDefinitions = [];
                        // filter out all SIGXXX constants
                        foreach ($this->constantDefinitions as $k => $v) {
                            if (
                                str_starts_with($k, 'SIG') &&
                                $k !== 'SIGSTKSZ' &&
                                $k !== 'SIGUNUSED' &&
                                $k !== 'SIGRTMIN' &&
                                $k !== 'SIGRTMAX'
                            ) {
                                $signalDefinitions[substr($k, 3)] = $v;
                            }
                        }
                        foreach ($node->stmts as $i => $stmt) {
                            if (!$stmt instanceof Node\Stmt\ClassConst) {
                                continue;
                            }
                            // entering class const Swow\Signal::xxx
                            // note: swow stubs' const declaration only one constant per statment so we can do this
                            $name = $stmt->consts[0]->name->name;
                            if (($constantDefinition = $signalDefinitions[$name] ?? null) === null) {
                                // not a signal name, skip it
                                continue;
                            }
                            unset($signalDefinitions[$name], $node->stmts[$i]);
                            // remove old things

                            // replace it with fixed value
                            $oldConst = $stmt->consts[0];
                            $oldConst->value = new Node\Scalar\LNumber(
                                value: $constantDefinition->value,
                            );
                            $newStmt = new Node\Stmt\ClassConst(
                                consts: [
                                    $oldConst,
                                ],
                                flags: $stmt->flags,
                                attributes: [
                                    'comments' => [
                                        new Doc(
                                            text: "/** \n * This constant holds SIG{$name} value, it's platform-dependent.\n * " .
                                                str_replace("\n", "\n * ", $constantDefinition->comment) .
                                                "\n */",
                                        ),
                                    ],
                                ]
                            );
                            $newStmts[] = [
                                'name' => $name,
                                'value' => $constantDefinition->value,
                                'stmt' => $newStmt,
                            ];
                        }
                        foreach ($signalDefinitions as $name => $constantDefinition) {
                            $newStmt = new Node\Stmt\ClassConst(
                                consts: [
                                    new Node\Const_(
                                        name: new Node\Identifier(
                                            name: $name
                                        ),
                                        value: new Node\Scalar\LNumber(
                                            value: $constantDefinition->value
                                        ),
                                    ),
                                ],
                                flags: Node\Stmt\Class_::MODIFIER_PUBLIC,
                                attributes: [
                                    'comments' => [
                                        new Doc(
                                            text: "/** \n * This constant holds SIG{$name} value, it's platform-dependent.\n * " .
                                                str_replace("\n", "\n * ", $constantDefinition->comment) .
                                                "\n */",
                                        ),
                                    ],
                                ]
                            );
                            $newStmts[] = [
                                'name' => $name,
                                'value' => $constantDefinition->value,
                                'stmt' => $newStmt,
                            ];
                        }
                        // sort all signals
                        uasort($newStmts, static fn ($x, $y) => $x['value'] - $y['value'] === 0 ? strcmp($x['name'], $y['name']) : $x['value'] - $y['value']);

                        // prepend to original class
                        //print_r(array_values($newStmts));
                        $node->stmts = [...array_map(static fn ($x) => $x['stmt'], $newStmts), ...$node->stmts];
                    } elseif (
                        $node instanceof Node\Stmt\Class_ &&
                        $this->inNamespaceClass('Swow', 'Socket')
                    ) {
                        // entering class Swow\Socket
                        $unix_included = false;
                        $udg_included = false;
                        $last = 0;
                        $lastFlag = 0;
                        foreach ($node->stmts as $i => $stmt) {
                            if (
                                !$unix_included &&
                                $stmt instanceof Node\Stmt\ClassConst &&
                                $stmt->consts[0]->name->name === 'TYPE_UNIX'
                            ) {
                                $unix_included = true;
                                continue;
                            }
                            if (
                                !$udg_included &&
                                $stmt instanceof Node\Stmt\ClassConst &&
                                $stmt->consts[0]->name->name === 'TYPE_UDG'
                            ) {
                                $udg_included = true;
                                continue;
                            }
                            if (
                                $stmt instanceof Node\Stmt\ClassConst &&
                                str_starts_with($stmt->consts[0]->name->name, 'TYPE_')
                            ) {
                                $last = $i;
                                $lastFlag = $stmt->flags;
                            }
                        }
                        $appending = [];
                        if (!$unix_included) {
                            $appending[] = new Node\Stmt\ClassConst(
                                consts: [
                                    new Node\Const_(
                                        name: 'TYPE_UNIX',
                                        value: new Node\Scalar\LNumber(
                                            value: 33554561,
                                        )
                                    ),
                                ],
                                flags: $lastFlag,
                                attributes: [
                                    'comments' => [
                                        new Doc(
                                            text: '/** UNIX domain socket type, this constant is only available at unix-like os. */'
                                        ),
                                    ],
                                ]
                            );
                        }
                        if (!$udg_included) {
                            $appending[] = new Node\Stmt\ClassConst(
                                consts: [
                                    new Node\Const_(
                                        name: 'TYPE_UDG',
                                        value: new Node\Scalar\LNumber(
                                            value: 268435586,
                                        )
                                    ),
                                ],
                                flags: $lastFlag,
                                attributes: [
                                    'comments' => [
                                        new Doc(
                                            text: "/** \n * UNIX datagram socket type, this constant is only avaliable at unix-like os.\n */"
                                        ),
                                    ],
                                ]
                            );
                        }
                        // insert
                        array_splice(
                            array: $node->stmts,
                            offset: $last + 1,
                            length: 0,
                            replacement: $appending
                        );
                    } elseif (
                        $node instanceof Node\Stmt\ClassConst &&
                        $this->inNamespaceClass('Swow', 'Buffer') &&
                        $node->consts &&
                        $node->consts[0]->name->name === 'PAGE_SIZE'
                    ) {
                        // entering class const Swow\Buffer::PAGE_SIZE
                        // note: swow stubs' const declaration only one constant per statment so we can do this
                        // comment is readonly attribute, so we need create a new ClassConst object to replace it
                        $oldStmt = $node;
                        $oldConst = $node->consts[0];
                        // replace it with fixed value
                        $constantDefinition = $this->constantDefinitions['PAGE_SIZE'];
                        $oldConst->value = new Node\Scalar\LNumber(
                            value: $constantDefinition->value,
                        );
                        if ($constantDefinition->comment !== '') {
                            $attr = [
                                'comments' => [
                                    new Doc(
                                        text: "/** \n * This constant holds page size of this machine, it's platform-dependent.\n *" .
                                            str_replace("\n", "\n * ", $constantDefinition->comment) .
                                            "\n */",
                                    ),
                                ],
                            ];
                        } else {
                            $attr = [];
                        }
                        $newStmt = new Node\Stmt\ClassConst(
                            consts: [
                                $oldConst,
                            ],
                            flags: $oldStmt->flags,
                            attributes: $attr,
                        );
                        //print_r($newStmt);
                        return $newStmt;
                    } elseif (
                        $node instanceof Node\Stmt\ClassConst &&
                        $this->inNamespaceClass('Swow', 'Errno')
                    ) {
                        // entering class const Swow\Errno::xxx
                        // note: swow stubs' const declaration only one constant per statment so we can do this
                        // comment is readonly attribute, so we need create a new ClassConst object to replace it
                        $oldStmt = $node;
                        $oldConst = $node->consts[0];
                        $name = $oldConst->name->name;

                        if (!($constantDefinition = $this->constantDefinitions["UV_{$name}"] ?? null)) {
                            //printf("cannot find errno %s\n", $name);
                            return null;
                        }
                        // replace it with fixed value
                        $oldConst->value = new Node\Scalar\LNumber(
                            value: $constantDefinition->value,
                        );
                        if ($constantDefinition->comment !== '') {
                            $attr = [
                                'comments' => [
                                    new Doc(
                                        text: "/** \n * This constant holds UV_{$name} value, it's platform-dependent.\n *" .
                                            str_replace("\n", "\n * ", $constantDefinition->comment) .
                                            "\n */",
                                    ),
                                ],
                            ];
                        } else {
                            $attr = [];
                        }
                        $newStmt = new Node\Stmt\ClassConst(
                            consts: [
                                $oldConst,
                            ],
                            flags: $oldStmt->flags,
                            attributes: $attr,
                        );
                        //print_r($newStmt);
                        return $newStmt;
                    }
                    return null;
                }
            });
            $traverser->traverse($ast);

            $prettyPrinter = new StubPrettierPrinter();
            return $prettyPrinter->prettyPrintFile($ast);
        },
    ],
);

$fixer->fix(fileName: __DIR__ . '/../lib/swow-stub/src/Swow.php', dryRun: false);
