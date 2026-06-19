--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Recursion through eval() is bounded by the depth cap (no native-stack overflow)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// eval()/include/require recurse in C off the main OP_CALL path; without the
// VmEvalChunk guard a self-eval chain would overflow the native stack and
// reboot a small-stack embedder. It must instead hit the same cap and halt.
function r() { eval("r();"); }
r();
echo "AFTER\n";
?>
--EXPECTF--
%s Error:  Maximum recursion depth of %d reached
