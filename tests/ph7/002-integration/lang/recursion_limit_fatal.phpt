--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A configured PHP call-depth cap raises a clean fatal and halts (not a silent NULL, not a panic)
--DESCRIPTION--
The host default is UNBOUNDED (PHP frames are heap-bound since the iterative
executor — BYTECODE.md stage 5), so this test installs a cap via the
PHL_MAX_RECURSION knob (-> PH7_VM_CONFIG_RECURSION_DEPTH) and checks that
exceeding it is a clean, non-catchable fatal that still runs shutdown handlers.
phl-only: the cap is engine policy real php does not expose the same way.
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip phl-only: exercises an engine-internal configured PHP-depth cap, not expressible in php'; ?>
--ENV--
PHL_MAX_RECURSION=64
--FILE--
<?php
// Unconditional self-recursion trips the configured depth cap. The old engine
// silently returned NULL and kept running; it must raise a fatal and halt —
// the statement after the call must NOT execute, and a shutdown callback must
// still run (clean unwind, never a native-stack panic).
register_shutdown_function(function () { echo "SHUTDOWN\n"; });
function g() { g(); }
g();
echo "AFTER\n";
?>
--EXPECTF--
%s Error:  Maximum recursion depth of 64 reached
SHUTDOWN
