--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
usort/uasort are stable: equal elements keep their original order (PHP 8.0+)
--DESCRIPTION--
Regression: the linked-list merge sort combined its accumulator buckets with the
later run as the LEFT operand, and the merge favors its left operand on a tie, so
equal-keyed elements were reversed (five tie-keyed items sorted to
last,first,second,third,fourth). Higher buckets hold earlier-inserted runs, so
they must be the left operand — restoring php's guaranteed sort stability across
the usort/uasort/sort family.
--FILE--
<?php
$data = [];
foreach (['first','second','third','fourth','fifth'] as $name) $data[] = ['prio' => 0, 'name' => $name];
usort($data, fn($a, $b) => $a['prio'] <=> $b['prio']);
echo implode(',', array_map(fn($r) => $r['name'], $data)), "\n";

$m = ['k1' => 1, 'k2' => 1, 'k3' => 1, 'k4' => 0];
uasort($m, fn($a, $b) => $a <=> $b);
echo implode(',', array_keys($m)), "\n";

$p = [];
foreach (['a','b','c','d','e','f'] as $i => $x) $p[] = [$i % 2, $x];
usort($p, fn($a, $b) => $a[0] <=> $b[0]);
echo implode(',', array_map(fn($r) => $r[1], $p)), "\n";
?>
--EXPECT--
first,second,third,fourth,fifth
k4,k1,k2,k3
a,c,e,b,d,f
--CLEAN--
<?php
