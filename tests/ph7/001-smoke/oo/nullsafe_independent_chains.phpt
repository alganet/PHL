--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Two nullsafe chains in the same expression short-circuit independently
--FILE--
<?php
class NsfIndepNum { public $v = 10; }
$nsfIndep_a = null;
$nsfIndep_b = new NsfIndepNum();
$nsfIndep_sum = ($nsfIndep_a?->v ?? 0) + ($nsfIndep_b?->v ?? 0);
echo $nsfIndep_sum, "\n";
$nsfIndep_a2 = new NsfIndepNum();
$nsfIndep_b2 = null;
$nsfIndep_sum2 = ($nsfIndep_a2?->v ?? 0) + ($nsfIndep_b2?->v ?? 0);
echo $nsfIndep_sum2, "\n";
?>
--EXPECT--
10
10
--CLEAN--
<?php
unset($nsfIndep_a, $nsfIndep_b, $nsfIndep_a2, $nsfIndep_b2, $nsfIndep_sum, $nsfIndep_sum2);
