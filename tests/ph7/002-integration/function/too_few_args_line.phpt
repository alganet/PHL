--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ArgumentCountError call-site line: PHL reports a fixed line 1 pending the runtime line-tracking gate (NEWPLAN.md §6; php half in the _zend twin)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
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
Too few arguments to function tflLine(), 0 passed in %s on line 1 and exactly 1 expected
--CLEAN--
<?php
unset($e);
