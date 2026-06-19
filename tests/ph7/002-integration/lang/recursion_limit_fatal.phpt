--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Recursion limit raises a clean fatal and halts (not a silent NULL, not a panic)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Unconditional self-recursion trips the depth cap regardless of its value.
// The old engine silently returned NULL and kept running; it must now raise a
// fatal and halt — the statement after the call must NOT execute, and a
// shutdown callback must still run (clean unwind, never a native-stack panic).
register_shutdown_function(function () { echo "SHUTDOWN\n"; });
function g() { g(); }
g();
echo "AFTER\n";
?>
--EXPECTF--
%s Error:  Maximum recursion depth of %d reached
SHUTDOWN
