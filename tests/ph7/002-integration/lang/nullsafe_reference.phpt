--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Reference operator cannot target a nullsafe chain
--FILE--
<?php
class NsfRefBox { public $v = 1; }
$nsfRef_a = new NsfRefBox();
$nsfRef_x = &$nsfRef_a?->v;
echo "never\n";
?>
--EXPECTF--
%Anullsafe%A
--CLEAN--
<?php
