--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An increment through a nullsafe chain is refused
--FILE--
<?php
// The `++`/`--` path never asked the nullsafe question, so this RAN.
class WtNullsafeIncr { public $p = 1; }
$o = new WtNullsafeIncr;
$o?->p++;
?>
--EXPECTF--
%ACan't use nullsafe operator in write context%A
