--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto from one try into a SIBLING try: php leaves the first (running its finally) and lands inside the second with its handler live (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
try { echo "A\n"; goto lbl; } finally { echo "A fin\n"; }
try { echo "B\n"; lbl: throw new Exception("x"); }
catch (Exception $e) { echo "B caught: ", $e->getMessage(), "\n"; }
finally { echo "B fin\n"; }
echo "end\n";
?>
--EXPECT--
A
A fin
B caught: x
B fin
end
--CLEAN--
<?php
