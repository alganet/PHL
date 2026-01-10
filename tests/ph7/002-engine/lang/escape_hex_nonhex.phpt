--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Hex escape sequence with non-hex digit
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test \x followed by non-hex to cover else branch in PH7_CompileString

$literal_x = "\xz";
if ($literal_x === "xz") {
    echo "Literal xz ok\n";
}

$literal_x2 = "\xG";
if ($literal_x2 === "xG") {
    echo "Literal xG ok\n";
}

$valid_hex = "\x41";
if ($valid_hex === "A") {
    echo "Valid hex ok\n";
}
?>
--EXPECT--
Literal xz ok
Literal xG ok
Valid hex ok