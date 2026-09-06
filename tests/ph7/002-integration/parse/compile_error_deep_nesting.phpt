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
%AParse error:%AUnclosed '{' on line 11%AParse error:%AUnclosed '{' on line 10%AParse error:%AUnclosed '{' on line 9%AParse error:%AUnclosed '{' on line 8%AParse error:%AUnclosed '{' on line 7%AParse error:%AUnclosed '{' on line 6%AParse error:%AUnclosed '{' on line 5%AParse error:%AUnclosed '{' on line 4%AParse error:%AUnclosed '{' on line 3%A
--CLEAN--
<?php

