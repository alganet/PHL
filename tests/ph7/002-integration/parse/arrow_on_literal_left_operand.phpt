--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
"->" on a NUMBER literal is a PHL compile fatal where php is a parse error — and must not double-free the expression tree
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip php reports its own parse error; see the _zend twin'; ?>
--FILE--
<?php
// Both engines refuse this one: a NUMBER is not php's `dereferencable` (the
// string/array/constant forms ARE, and both engines run them — see
// 001-smoke/lang/deref_scalar_left_operand.phpt). Only the diagnostic KIND
// differs, which is the recorded parse-error-kind divergence.
//
// Rejecting the left operand used to link it into the parent node BEFORE the
// check, leaving it reachable from BOTH the node set and pNode->pLeft — so the
// expression-tree teardown freed it twice (a heap-use-after-free ASan caught on
// this very source). Running this file at all is the regression guard; the
// message below only pins the diagnostic.
1->x;
?>
--EXPECTF--
%A'->': Expecting a variable as left operand%A
