--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Octal escape sequence with value 0
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test \o0 to cover if (c > 0) false branch in PH7_CompileString

$zero = "\o0";
if (strlen($zero) == 0) {
    echo "Zero octal ok\n";
}

$zero_pad = "\o00";
if (strlen($zero_pad) == 0) {
    echo "Zero octal padded ok\n";
}

$one = "\o1";
if (ord($one) === 1) {
    echo "One octal ok\n";
}
?>
--EXPECT--
Zero octal ok
Zero octal padded ok
One octal ok
--CLEAN--
<?php
unset($zero, $zero_pad, $one);
