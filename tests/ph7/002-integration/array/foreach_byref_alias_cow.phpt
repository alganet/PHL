--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Value alias taken INSIDE a by-ref foreach: PHL's COW hands the writer a fresh map, detaching the loop (php follows the variable live — divergence recorded in NEWPLAN.md §7)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
$a = [1, 2];
foreach ($a as &$v) {
    if ($v == 1) {
        $b = $a;
        $a[] = 3;
    }
    $v *= 100;
}
unset($v);
echo implode(",", $a), " | ", implode(",", $b), "\n";
?>
--EXPECT--
1,2,3 | 100,200
--CLEAN--
<?php
unset($a, $b);
