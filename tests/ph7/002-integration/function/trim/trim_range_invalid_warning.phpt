--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
PH7: malformed `..`-range charlists emit PHP-exact warning text
--FILE--
<?php
// Each malformed range emits a warning naming the calling function, then the
// call still returns a result built from the surrounding bytes as literals.
echo trim("aXbz", "z..a"), "\n";   // wrong order
echo trim("a.b.", "a.."), "\n";    // no char to the right
echo trim(".a.b", "..b"), "\n";    // no char to the left
echo rtrim("Zx", "y..a"), "\n";    // wrong order in rtrim()
echo addcslashes("aZ", "Z..A"), "\n"; // wrong order in addcslashes()
?>
--EXPECTF--
%s Warning:  trim(): Invalid '..'-range, '..'-range needs to be incrementing
Xb
%s Warning:  trim(): Invalid '..'-range, no character to the right of '..'
b
%s Warning:  trim(): Invalid '..'-range, no character to the left of '..'
a
%s Warning:  rtrim(): Invalid '..'-range, '..'-range needs to be incrementing
Zx
%s Warning:  addcslashes(): Invalid '..'-range, '..'-range needs to be incrementing
a\Z
--CLEAN--
<?php
