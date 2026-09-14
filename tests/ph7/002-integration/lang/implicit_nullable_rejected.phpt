--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Implicit-nullable parameter (int $x = null) is a hard compile error (php deprecates; PHL removes)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip PHL removes what php only deprecates'; ?>
--FILE--
<?php
// php 8.4 DEPRECATES the implicit-nullable form `int $x = null` (still runs it);
// PHL targets php's non-deprecated surface, so it is a hard compile error. The
// explicit `?int` must be written instead.
function inpF(int $x = null) { return $x; }
echo "unreachable";
?>
--EXPECTF--
%AFatal error:%AinpF(): Cannot use null as the default for non-nullable parameter $x; write the explicit ?T type instead%A
