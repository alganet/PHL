--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto INTO a try/catch body is a PHL compile error (php allows it — a recorded divergence; PHL-pinned half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
goto inside;
try { inside: echo "t;"; } finally { echo "f;"; }
echo "|end\n";
?>
--EXPECTF--
%A'goto' into a try, catch or finally block is disallowed%A
--CLEAN--
<?php
