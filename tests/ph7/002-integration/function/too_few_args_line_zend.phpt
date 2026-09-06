--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ArgumentCountError call-site line: php embeds the REAL call-site line (zend half of the twin pair — the line-tracking gate)
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
