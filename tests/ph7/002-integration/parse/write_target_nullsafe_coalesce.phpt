--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A `??=` through a nullsafe chain is refused
--FILE--
<?php
// `??=` compiles its own way and never reached the write-target check.
class WtNullsafeCoal { public $p; }
$o = new WtNullsafeCoal;
$o?->p ??= 3;
?>
--EXPECTF--
%ACan't use nullsafe operator in write context%A
