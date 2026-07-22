--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ArgumentCountError call-site line: php embeds the REAL call-site line (zend half of the twin pair — NEWPLAN.md §6 line-tracking gate)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
function tflLine($a) {}
try {
    tflLine();
} catch (ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECTF--
Too few arguments to function tflLine(), 0 passed in %s on line 4 and exactly 1 expected
--CLEAN--
<?php
unset($e);
