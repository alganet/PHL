--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A write through an internal function's result is refused (php 8.5 clone)
--FILE--
<?php
// php decides this at COMPILE time, so the refusal arrives even for a method
// nobody calls. PHL used to perform it: $this became an int for the rest of the
// call and every later $this->x failed somewhere else entirely.
class WriteTargetClone { public $p; }
$o = new WriteTargetClone;
(clone $o)->p = 1;
?>
--EXPECTF--
%ACannot use result of built-in function in write context%A
