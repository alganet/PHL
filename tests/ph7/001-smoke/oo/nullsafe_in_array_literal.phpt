--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe in array literal — each element is an independent scope
--FILE--
<?php
class NsfArrLitBox { public $v = 7; }
$nsfArrLit_a = null;
$nsfArrLit_b = new NsfArrLitBox();
$nsfArrLit_out = array($nsfArrLit_a?->v, $nsfArrLit_b?->v, $nsfArrLit_a?->v ?? "fallback");
echo ($nsfArrLit_out[0] === null ? "null" : "no"), "\n";
echo $nsfArrLit_out[1], "\n";
echo $nsfArrLit_out[2], "\n";
?>
--EXPECT--
null
7
fallback
--CLEAN--
<?php
unset($nsfArrLit_a, $nsfArrLit_b, $nsfArrLit_out);
