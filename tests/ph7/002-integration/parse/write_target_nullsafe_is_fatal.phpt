--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A nullsafe chain in write context is a compile FATAL
--FILE--
<?php
// php reports this as a fatal, not a parse error; PHL said "Parse error".
class WriteTargetNullsafe { public $p; }
$o = new WriteTargetNullsafe;
$o?->p = 1;
?>
--EXPECTF--
%AFatal error:%ACan't use nullsafe operator in write context%A
