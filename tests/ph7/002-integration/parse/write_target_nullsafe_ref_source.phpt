--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A reference SOURCE has php's own nullsafe sentence
--FILE--
<?php
class WtNullsafeRef { public $p = 1; }
$o = new WtNullsafeRef;
$r =& $o?->p;
?>
--EXPECTF--
%ACannot take reference of a nullsafe chain%A
