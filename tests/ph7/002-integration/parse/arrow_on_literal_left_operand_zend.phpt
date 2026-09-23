--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php reports a PARSE error for "->" on a number literal (zend half of the twin pair — PHL compile-fatals with its own wording, see arrow_on_literal_left_operand.phpt)
--SKIPIF--
<?php if (!function_exists('zend_version')) echo 'skip zend half of the twin pair; PHL half is arrow_on_literal_left_operand.phpt'; ?>
--FILE--
<?php
// php's `dereferencable` covers a string, an array literal, a parenthesised
// expression and a constant — all of which BOTH engines now run — but not a
// number, which is a parse error here and a compile fatal in PHL.
1->x;
?>
--EXPECTF--
%Asyntax error, unexpected token "->"%A
