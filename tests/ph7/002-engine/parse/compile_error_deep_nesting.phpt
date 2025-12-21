--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test compilation error with deep nesting to cover uncovered lines in compile.c
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Deep nesting to trigger allocation failure in GenStateEnterBlock
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
if (true) {
echo "deep";
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
?>
--EXPECTF--
%s 11 Error: Missing closing braces '}'
%s 10 Error: Missing closing braces '}'
%s 9 Error: Missing closing braces '}'
%s 8 Error: Missing closing braces '}'
%s 7 Error: Missing closing braces '}'
%s 6 Error: Missing closing braces '}'
%s 5 Error: Missing closing braces '}'
%s 4 Error: Missing closing braces '}'
%s 3 Error: Missing closing braces '}'
Compile error
