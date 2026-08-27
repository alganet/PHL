--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Recursion through eval() is bounded by the NATIVE-nesting cap (no native-stack overflow)
--DESCRIPTION--
eval()/include/require recurse in C off the main OP_CALL trampoline
(VmEvalChunk -> VmByteCodeExec), a path the iterative executor never
flattened. PHP call depth is now unbounded, so what stops a self-eval chain
from overflowing the native stack is the separate native-nesting cap
(PH7_VM_CONFIG_NATIVE_DEPTH). Here it is lowered to 32 via PHL_MAX_NATIVE_DEPTH
so the fatal fires quickly and deterministically. This pins the CONFIG-KNOB path
(PHL_MAX_NATIVE_DEPTH -> PH7_VM_CONFIG_NATIVE_DEPTH); its deep-tier sibling
tests/ph7/004-deep/native_cap_eval.phpt pins the same fatal at the SHIPPED
default (256, no env) — the pair is deliberate, not redundant. phl-only: an
engine-internal cap real php does not express.
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip phl-only: engine-internal native-nesting cap, not expressible in php'; ?>
--ENV--
PHL_MAX_NATIVE_DEPTH=32
--FILE--
<?php
function r() { eval("r();"); }
r();
echo "AFTER\n";
?>
--EXPECTF--
Error: Maximum native nesting depth reached in %s on line %d
