--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: for() clauses keep php's comma-separated expression lists
--FILE--
<?php
// init and post clauses take comma-separated lists (php's grammar allows this
// ONLY here — there is no comma operator elsewhere)
for ($i = 0, $j = 10; $i < 3; $i++, $j--) {
}
echo $i, "|", $j, "\n";

// commas elsewhere are separators, not operators: still fine
function fccSum($a, $b) { return $a + $b; }
$arr = [1, 2, 3];
echo fccSum(1, 2), "|", count($arr), "|", max(4, 5), "\n";
echo isset($arr, $i) ? "isset-ok" : "isset-no", "\n";
list($p, $q) = [7, 8];
echo $p, $q, "\n";
?>
--EXPECT--
3|7
3|3|5
isset-ok
78
--CLEAN--
<?php
unset($i, $j, $arr, $p, $q);
