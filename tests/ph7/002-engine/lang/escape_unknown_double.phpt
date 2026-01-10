--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Unknown escape sequences in double quoted strings
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test unknown escapes in double quoted strings to cover default case in PH7_CompileString

$unknown1 = "\z";
if ($unknown1 === "z") {
    echo "Unknown z ok\n";
}

$unknown2 = "\q";
if ($unknown2 === "q") {
    echo "Unknown q ok\n";
}

$unknown3 = "\1";
if ($unknown3 === "1") {
    echo "Unknown 1 ok\n";
}

$valid_n = "\n";
if ($valid_n === "\x0A") {
    echo "Valid newline ok\n";
}
?>
--EXPECT--
Unknown z ok
Unknown q ok
Unknown 1 ok
Valid newline ok