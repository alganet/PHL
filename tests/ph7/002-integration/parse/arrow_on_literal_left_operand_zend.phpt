--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php accepts "->" on a literal left operand and warns at runtime (zend half of the twin pair — PHL compile-fatals, see arrow_on_literal_left_operand.phpt)
--SKIPIF--
<?php if (!function_exists('zend_version')) echo 'skip zend half of the twin pair; PHL half is arrow_on_literal_left_operand.phpt'; ?>
--FILE--
<?php
// php's grammar takes any dereferenceable expression on the left of "->": a
// string, array or parenthesised scalar literal parses and yields null with a
// warning. PHL's parser demands a variable-ish term and rejects at compile time.
var_dump("s"->x);
var_dump([1]->x);
var_dump((1)->x);
?>
--EXPECTF--
%AAttempt to read property "x" on string%A
NULL
%AAttempt to read property "x" on array%A
NULL
%AAttempt to read property "x" on int%A
NULL
