--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A write INTO a call's result names the kind of call
--FILE--
<?php
// php lets a call be written THROUGH (`f()[0] = 5` runs) but never INTO. The
// two are decided in different places in php, and only the second is refused —
// with a wording that says which kind of call it was. PHL refused both alike,
// with a message naming the operator instead.
function writeTargetCallDirect() { return [1,2]; }
writeTargetCallDirect() = 5;
?>
--EXPECTF--
%ACan't use function return value in write context%A
