--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
const with reserved names
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
const null = 1;
const true = 2;
const false = 3;
echo "Should not reach here\n";
?>
--EXPECTF--
%AFatal error:%ACannot redeclare constant 'null'%AFatal error:%ACannot redeclare constant 'true'%AFatal error:%ACannot redeclare constant 'false'%A
--CLEAN--
<?php

