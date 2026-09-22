--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto INTO a try body: php's handlers are instruction RANGES, so landing mid-body is being inside the try and the finally still runs (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
goto inside;
try { inside: echo "t;"; } finally { echo "f;"; }
echo "|end\n";
?>
--EXPECT--
t;f;|end
--CLEAN--
<?php
