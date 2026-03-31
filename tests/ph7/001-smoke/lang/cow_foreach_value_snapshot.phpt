--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
COW: foreach by-value iterates snapshot, body can modify original
--FILE--
<?php
$a = [1, 2, 3];
$out = [];
foreach ($a as $v) {
    $a[0] = 99;
    $out[] = $v;
}
echo $a[0] . "\n";
echo implode(",", $out) . "\n";
?>
--EXPECT--
99
1,2,3
--CLEAN--
<?php
unset($a, $out);
