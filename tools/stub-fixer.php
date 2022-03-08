#!/usr/bin/env php
<?php
/**
 * This file is part of Swow
 * OS-arch-dependent constant fixer and stub formatter
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

/**
 * swow specified constant modifier
 *
 * this modifier will:
 *
 * - add array shape comments to __debugInfo
 * - fix value and generate document for Swow\Signal constants
 * - fix value and generate document for Swow\Errno constants
 * - fix value and generate document for Swow\Buffer::PAGE_SIZE constant
 * - prepend definition and generate document for Swow\Socket::{TYPE_UDG, TYPE_UNIX} constants
 * - remove all ending spaces
 */
$swowModifier = function (
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
                $node instanceof Node\Stmt\ClassMethod &&
                $node->name->name === '__debugInfo'
            ) {
                // entering any __debugInfo
                $node->setAttribute('comments', [
                    new Doc('/** @return array<string, mixed> debug information for var_dump */'),
                ]);
            } elseif (
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
                // process all const exists
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
                                    text: "/**\n * This constant holds SIG{$name} value, it's platform-dependent.\n *" .
                                        implode("\n *", array_map(static fn ($x) => $x === '' ? $x : " {$x}", explode("\n", $constantDefinition->comment))) .
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
                                    text: "/**\n * This constant holds SIG{$name} value, it's platform-dependent.\n *" .
                                        implode("\n *", array_map(static fn ($x) => $x === '' ? $x : " {$x}", explode("\n", $constantDefinition->comment))) .
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
                                    text: '/** UNIX datagram socket type, this constant is only avaliable at unix-like os. */'
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
                $const = $node->consts[0];
                // replace it with fixed value
                $constantDefinition = $this->constantDefinitions['PAGE_SIZE'];
                $const->value = new Node\Scalar\LNumber(
                    value: $constantDefinition->value,
                );
                if ($constantDefinition->comment !== '') {
                    $const->setAttribute(
                        'comments',
                        [
                            new Doc(
                                text: "/**\n * This constant holds page size of this machine, it's platform-dependent.\n *" .
                                    implode("\n *", array_map(static fn ($x) => $x === '' ? $x : " {$x}", explode("\n", $constantDefinition->comment))) .
                                    "\n */",
                            ),
                        ]
                    );
                }
            } elseif (
                $node instanceof Node\Stmt\ClassConst &&
                $this->inNamespaceClass('Swow', 'Errno')
            ) {
                // entering class const Swow\Errno::xxx
                // note: swow stubs' const declaration only one constant per statment so we can do this
                $const = $node->consts[0];
                $name = $const->name->name;

                if (!($constantDefinition = $this->constantDefinitions["UV_{$name}"] ?? null)) {
                    //printf("cannot find errno %s\n", $name);
                    return null;
                }
                // replace it with fixed value
                $const->value = new Node\Scalar\LNumber(
                    value: $constantDefinition->value,
                );
                if ($constantDefinition->comment !== '') {
                    $const->setAttribute(
                        'comments',
                        [
                            new Doc(
                                text: "/**\n * This constant holds UV_{$name} value, it's platform-dependent.\n *" .
                                    implode("\n *", array_map(static fn ($x) => $x === '' ? $x : " {$x}", explode("\n", $constantDefinition->comment))) .
                                    "\n */",
                            ),
                        ],
                    );
                }
            }
            return null;
        }
    });
    $traverser->traverse($ast);

    $prettyPrinter = new StubPrettierPrinter();
    return preg_replace('/ +\n/', "\n", $prettyPrinter->prettyPrintFile($ast));
};

(static function () use ($argv, $swowModifier): void {
    $options = getopt('hn', ['help', 'dry-run'], $restIndex);
    $argv = \array_slice($argv, $restIndex);
    $filename = $argv[0] ?? '';
    $dryRun = false;
    if (isset($options['h']) || isset($options['help']) || empty($filename)) {
        $basename = basename(__FILE__);
        $msg = <<<HELP
Usage:
    php {$basename} [-n|--dry-run] <file>
    php {$basename} [-h|-help]
Options:
    -n/--dry-run: donot change file content, output fixed file to stdout
    -h/--help: show this help
Environments:
    https_proxy: specify used proxy when download headers from github
HELP;
        fprintf(\STDERR, '%s', $msg);
        exit(1);
    }

    if (!is_file($filename)) {
        fprintf(\STDERR, "target '%s' is not a file\n", $filename);
        exit(1);
    }

    if (isset($options['n']) || isset($options['dry-run'])) {
        $dryRun = true;
    }
    $fixer = new ConstantFixer(modifiers: [$swowModifier]);
    $fixer->fix(fileName: $filename, dryRun: $dryRun);
})();
