--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nullsafe jump inside a ternary branch is confined to that branch
--FILE--
<?php
class NsfTerScopePair { public $a = 1; public $b = 2; }
$nsfTerScope_p = null;
$nsfTerScope_q = new NsfTerScopePair();
$nsfTerScope_x = ($nsfTerScope_p ? $nsfTerScope_p?->a : $nsfTerScope_q?->b);
echo $nsfTerScope_x, "\n";
$nsfTerScope_y = ($nsfTerScope_q ? $nsfTerScope_q?->a : $nsfTerScope_p?->b);
echo $nsfTerScope_y, "\n";
?>
--EXPECT--
2
1
--CLEAN--
<?php
unset($nsfTerScope_p, $nsfTerScope_q, $nsfTerScope_x, $nsfTerScope_y);
