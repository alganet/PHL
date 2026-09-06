--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ArgumentCountError call-site line: PHL reports a fixed line 1 pending the runtime line-tracking gate (php half in the _zend twin)
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
%AToo few arguments to function tflLine(), 0 passed%A
--CLEAN--
<?php
unset($e);
