--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Large array literal as a class const / static / instance default (pool-realloc safe)
--FILE--
<?php
// A big array literal in a constant-expression init once triggered a
// heap use-after-free: the result slot was a pointer into the growable
// memobj pool, and building the array reallocated that pool mid-eval.
// Well past the ~227-element pool-realloc boundary here.
$n = 1000;
$parts = [];
for ($i = 0; $i < $n; $i++) { $parts[] = (string)$i; }
$src = implode(',', $parts);

eval("class LcadConst  { const M = [$src]; }");
eval("class LcadStatic { public static \$p = [$src]; }");
eval("class LcadInst   { public \$p = [$src]; }");

echo count(LcadConst::M), "\n";
echo LcadConst::M[0], ' ', LcadConst::M[$n - 1], "\n";
echo count(LcadStatic::$p), "\n";
$obj = new LcadInst();
echo count($obj->p), "\n";
echo array_sum(LcadConst::M), "\n";
?>
--EXPECT--
1000
0 999
1000
1000
499500
--CLEAN--
<?php
