--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test compilation error with deep nesting to cover uncovered lines in compile.c
--SKIPIF--
<?php
// php ABORTS at the first compile error; PHL keeps compiling and reports every one
// it finds (then "Error count limit reached" past 15). That is a deliberate engine
// difference, not a fidelity gap -- reporting the whole batch is more useful for an
// embedded engine -- so the two can never agree on this output. The FIRST error's
// text is what has to match php, and that is asserted by the single-error tests in
// this directory; this test exists to pin PHL's continuation behavior.
if (function_exists('zend_version')) { echo 'skip php aborts at the first compile error; PHL reports all (engine design)'; }
?>
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

