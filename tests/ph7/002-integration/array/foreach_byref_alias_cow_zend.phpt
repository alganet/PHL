--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Value alias taken INSIDE a by-ref foreach: php's by-ref loop follows $a live and the alias shares the current element's reference (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
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
100,200,300 | 100,2
--CLEAN--
<?php
unset($a, $b);
