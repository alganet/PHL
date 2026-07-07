--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Recursive eval() hits the native-nesting fatal at the shipped default, not a C-stack overflow (BYTECODE.md stage 5)
--DESCRIPTION--
eval/include recurse on the native C stack (VmEvalChunk -> VmByteCodeExec),
a path the OP_CALL trampoline never flattened. PHP call depth is unbounded by
default (stage 5), so the separate native-nesting guard is what stops this — at
its SHIPPED default (256 host), with no env override, proving the default
protects the C stack. phl-only: this pins an engine-internal cap fatal, which
real php cannot express.
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip phl-only: engine-internal native-nesting cap, not expressible in php'; ?>
--FILE--
<?php
function r(int $n): int { return $n === 0 ? 0 : 1 + eval("return r(" . ($n - 1) . ");"); }
echo r(50000), "\n";
?>
--EXPECTF--
%s Error:  Maximum native nesting depth reached
--CLEAN--
<?php
