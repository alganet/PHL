--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Recursive eval() hits the native-nesting fatal, not a C-stack overflow (BYTECODE.md stage 3)
--DESCRIPTION--
eval/include recurse on the native C stack (VmEvalChunk -> VmByteCodeExec),
a path the OP_CALL trampoline never flattened. With the PHP-depth cap raised,
the separate native-nesting guard must stop it cleanly. phl-only: this pins an
engine-internal cap fatal, which real php cannot express.
--SKIPIF--
<?php if (!getenv('PHL_MAX_RECURSION')) { echo "skip needs PHL_MAX_RECURSION (run: make test-stress)"; } ?>
--FILE--
<?php
function r(int $n): int { return $n === 0 ? 0 : 1 + eval("return r(" . ($n - 1) . ");"); }
echo r(50000), "\n";
?>
--EXPECTF--
%s Error:  Maximum native nesting depth reached
--CLEAN--
<?php
