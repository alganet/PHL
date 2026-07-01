--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP 8.5: round() handles float-representation half-way traps byte-exact
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '8.5.0', '<')) echo 'skip Requires PHP 8.5+'; ?>
--FILE--
<?php
// Float-representation traps: the values the naive "+0.5" approach got wrong.
// The 8.4+ integer-extraction algorithm with error correction gets these
// right. Uses var_export for byte-exact float comparison.
$cases = [
    [0.285, 2], [1.005, 2], [2.675, 2], [1.955, 2], [1.55, 1], [9.995, 2],
    [0.045, 2], [0.145, 2], [-0.045, 2], [8.075, 2], [5.055, 2], [4.005, 2],
    [1.95583, 2], [0.1 + 0.2, 1], [2.6555, 3], [3.14159, 4], [1234.5678, 2],
];
foreach ($cases as [$v, $p]) {
    printf("round(%s, %d)=%s\n", var_export($v, true), $p, var_export(round($v, $p), true));
}
?>
--EXPECT--
round(0.285, 2)=0.29
round(1.005, 2)=1.01
round(2.675, 2)=2.68
round(1.955, 2)=1.96
round(1.55, 1)=1.6
round(9.995, 2)=10.0
round(0.045, 2)=0.05
round(0.145, 2)=0.15
round(-0.045, 2)=-0.05
round(8.075, 2)=8.08
round(5.055, 2)=5.06
round(4.005, 2)=4.01
round(1.95583, 2)=1.96
round(0.30000000000000004, 1)=0.3
round(2.6555, 3)=2.656
round(3.14159, 4)=3.1416
round(1234.5678, 2)=1234.57
