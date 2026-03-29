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
%s Fatal error:  Missing closing braces '}' %s
--CLEAN--
<?php

