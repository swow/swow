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

use PhpParser\ParserFactory;
use PhpParser\Node;
use PhpParser\NodeTraverser;
use PhpParser\Comment\Doc;

require __DIR__ . '/autoload.php';

/*
$data = '{"PAGE_SIZE":{"value":4096,"comment":"\nAt Linux mips64 and macOS arm64 platforms, this constant may have a value `16384`"},"SIGHUP":{"value":1,"comment":"\nAt macOS platform, this constant meaning \"hangup\"\nAt Windows platform, this constant may not exist"},"SIGINT":{"value":2,"comment":"\nAt macOS and Windows platforms, this constant meaning \"interrupt\""},"SIGQUIT":{"value":3,"comment":"\nAt macOS platform, this constant meaning \"quit\"\nAt Windows platform, this constant may not exist"},"SIGILL":{"value":4,"comment":"\nAt macOS platform, this constant meaning \"illegal instruction (not reset when caught)\"\nAt Windows platform, this constant meaning \"illegal instruction - invalid function image\""},"SIGTRAP":{"value":5,"comment":"\nAt macOS platform, this constant meaning \"trace trap (not reset when caught)\"\nAt Windows platform, this constant may not exist"},"SIGABRT":{"value":6,"comment":"\nAt macOS platform, this constant meaning \"abort()\"\nAt Windows platform, this constant may have a value `22` meaning \"abnormal termination triggered by abort call\""},"SIGIOT":{"value":6,"comment":"\nAt macOS and Windows platforms, this constant may not exist"},"SIGBUS":{"value":7,"comment":"\nAt Linux mips64 platform, this constant may have a value `10`\nAt macOS platform, this constant may have a value `10` meaning \"bus error\"\nAt Windows platform, this constant may not exist"},"SIGFPE":{"value":8,"comment":"\nAt macOS and Windows platforms, this constant meaning \"floating point exception\""},"SIGKILL":{"value":9,"comment":"\nAt macOS platform, this constant meaning \"kill (cannot be caught or ignored)\"\nAt Windows platform, this constant may not exist"},"SIGSEGV":{"value":11,"comment":"\nAt macOS platform, this constant meaning \"segmentation violation\"\nAt Windows platform, this constant meaning \"segment violation\""},"SIGPIPE":{"value":13,"comment":"\nAt macOS platform, this constant meaning \"write on a pipe with no one to read it\"\nAt Windows platform, this constant may not exist"},"SIGALRM":{"value":14,"comment":"\nAt macOS platform, this constant meaning \"alarm clock\"\nAt Windows platform, this constant may not exist"},"SIGTERM":{"value":15,"comment":"\nAt macOS platform, this constant meaning \"software termination signal from kill\"\nAt Windows platform, this constant meaning \"Software termination signal from kill\""},"SIGSTKFLT":{"value":16,"comment":"\nAt macOS and Windows platforms, this constant may not exist"},"SIGCHLD":{"value":17,"comment":"\nAt Linux mips64 platform, this constant may have a value `18`\nAt macOS platform, this constant may have a value `20` meaning \"to parent on child stop or exit\"\nAt Windows platform, this constant may not exist"},"SIGCONT":{"value":18,"comment":"\nAt Linux mips64 platform, this constant may have a value `25`\nAt macOS platform, this constant may have a value `19` meaning \"continue a stopped process\"\nAt Windows platform, this constant may not exist"},"SIGSTOP":{"value":19,"comment":"\nAt Linux mips64 platform, this constant may have a value `23`\nAt macOS platform, this constant may have a value `17` meaning \"sendable stop signal not from tty\"\nAt Windows platform, this constant may not exist"},"SIGTSTP":{"value":20,"comment":"\nAt Linux mips64 platform, this constant may have a value `24`\nAt macOS platform, this constant may have a value `18` meaning \"stop signal from tty\"\nAt Windows platform, this constant may not exist"},"SIGTTIN":{"value":21,"comment":"\nAt Linux mips64 platform, this constant may have a value `26`\nAt macOS platform, this constant meaning \"to readers pgrp upon background tty read\"\nAt Windows platform, this constant may not exist"},"SIGTTOU":{"value":22,"comment":"\nAt Linux mips64 platform, this constant may have a value `27`\nAt macOS platform, this constant meaning \"like TTIN for output if (tp->t_local&LTOSTOP)\"\nAt Windows platform, this constant may not exist"},"SIGURG":{"value":23,"comment":"\nAt Linux mips64 platform, this constant may have a value `21`\nAt macOS platform, this constant may have a value `16` meaning \"urgent condition on IO channel\"\nAt Windows platform, this constant may not exist"},"SIGXCPU":{"value":24,"comment":"\nAt Linux mips64 platform, this constant may have a value `30`\nAt macOS platform, this constant meaning \"exceeded CPU time limit\"\nAt Windows platform, this constant may not exist"},"SIGXFSZ":{"value":25,"comment":"\nAt Linux mips64 platform, this constant may have a value `31`\nAt macOS platform, this constant meaning \"exceeded file size limit\"\nAt Windows platform, this constant may not exist"},"SIGVTALRM":{"value":26,"comment":"\nAt Linux mips64 platform, this constant may have a value `28`\nAt macOS platform, this constant meaning \"virtual time alarm\"\nAt Windows platform, this constant may not exist"},"SIGPROF":{"value":27,"comment":"\nAt Linux mips64 platform, this constant may have a value `29`\nAt macOS platform, this constant meaning \"profiling time alarm\"\nAt Windows platform, this constant may not exist"},"SIGWINCH":{"value":28,"comment":"\nAt Linux mips64 platform, this constant may have a value `20`\nAt macOS platform, this constant meaning \"window size changes\"\nAt Windows platform, this constant may not exist"},"SIGIO":{"value":29,"comment":"\nAt Linux mips64 platform, this constant may have a value `22`\nAt macOS platform, this constant may have a value `23` meaning \"input\/output possible signal\"\nAt Windows platform, this constant may not exist"},"SIGLOST":{"value":29,"comment":"\nAt macOS and Windows platforms, this constant may not exist"},"SIGPWR":{"value":30,"comment":"\nAt Linux mips64 platform, this constant may have a value `19`\nAt macOS and Windows platforms, this constant may not exist"},"SIGSYS":{"value":31,"comment":"\nAt Linux mips64 platform, this constant may have a value `12`\nAt macOS platform, this constant may have a value `12` meaning \"bad argument to system call\"\nAt Windows platform, this constant may not exist"},"SIGUNUSED":{"value":31,"comment":"\nAt macOS and Windows platforms, this constant may not exist"},"SIGRTMIN":{"value":32,"comment":"\nAt macOS and Windows platforms, this constant may not exist"},"SIGSTKSZ":{"value":8192,"comment":"\nAt Linux arm64 platform, this constant may have a value `16384`\nAt macOS platform, this constant may have a value `131072` meaning \"(128K)recommended stack size\"\nAt Windows platform, this constant may not exist"},"SIGEMT":{"value":7,"comment":"\nAt macOS platform, this constant meaning \"EMT instruction\"\nAt Windows, Linux x86_64, Linux arm64 and Linux riscv64 platforms, this constant may not exist"},"SIGPOLL":{"value":7,"comment":"\nAt macOS platform, this constant meaning \"pollable event ([XSR] generated, not supported)\"\nAt Linux and Windows platforms, this constant may not exist"},"SIGINFO":{"value":29,"comment":"\nAt macOS platform, this constant meaning \"information request\"\nAt Linux and Windows platforms, this constant may not exist"},"SIGBREAK":{"value":21,"comment":"\nAt Windows platform, this constant meaning \"Ctrl-Break sequence\"\nAt Linux and macOS platforms, this constant may not exist"}}';

$a = json_decode($data, true);
foreach ($a as $i => &$e) {
    $e = new ConstantDefinition(
        value: $e['value'],
        comment: $e['comment'],
    );
}
*/

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
            $traverser->addVisitor(new class($constantDefinitions) extends SimpleNodeTraverserAbstract
            {
                public function __construct(
                    private array $constantDefinitions,
                ) {
                }
                public function enterNode(Node $node)
                {
                    parent::enterNode($node);
                    switch (true) {
                        case $node instanceof Node\Stmt\Class_:
                            $this->class = $node->name->name;
                            if (!$this->inNamespaceClass('Swow', 'Signal')) {
                                break;
                            }

                            /** @var array<array{'value': int, 'name': string, 'stmt': array<mixed>}> */
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
                                unset($signalDefinitions[$name]);
                                // remove old things
                                unset($node->stmts[$i]);
                                // replace it with fixed value
                                $oldConst = $stmt->consts[0];
                                $oldConst->value->value = $constantDefinition->value;
                                $newStmt = new Node\Stmt\ClassConst(
                                    consts: [
                                        $oldConst
                                    ],
                                    flags: $stmt->flags,
                                    attributes: [
                                        'comments' => [
                                            new Doc(
                                                text: "/** \n * This constant holds SIG$name value, it's platform-dependent.\n * " .
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
                                                text: "/** \n * This constant holds SIG$name value, it's platform-dependent.\n * " .
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
                            uasort($newStmts, fn ($x, $y) => $x['value'] - $y['value'] === 0 ? strcmp($x['name'], $y['name']) : $x['value'] - $y['value']);

                            // prepend to original class
                            //print_r(array_values($newStmts));
                            $node->stmts = [...array_map(fn ($x) => $x['stmt'], $newStmts), ...$node->stmts];
                            break;
                        case $node instanceof Node\Stmt\ClassConst &&
                            $this->inNamespaceClass('Swow', 'Buffer') &&
                            $node->consts &&
                            $node->consts[0]->name->name === 'PAGE_SIZE':
                            // entering class const Swow\Buffer::PAGE_SIZE
                            // note: swow stubs' const declaration only one constant per statment so we can do this
                            // comment is readonly attribute, so we need create a new ClassConst object to replace it
                            $oldStmt = $node;
                            $oldConst = $node->consts[0];
                            // replace it with fixed value
                            $constantDefinition = $this->constantDefinitions['PAGE_SIZE'];
                            $oldConst->value->value = $constantDefinition->value;
                            $newStmt = new Node\Stmt\ClassConst(
                                consts: [
                                    $oldConst
                                ],
                                flags: $oldStmt->flags,
                                attributes: [
                                    'comments' => [
                                        new Doc(
                                            text: "/** \n * This constant holds page size of this machine, it's platform-dependent." .
                                                str_replace("\n", "\n * ", $constantDefinition->comment) .
                                                "\n */",
                                        ),
                                    ],
                                ]
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
