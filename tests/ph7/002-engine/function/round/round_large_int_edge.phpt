--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
round: large integer edge cases
--FILE--
<?php
// Test rounding around large numbers; expect finite results and no crash
$v1 = pow(2, 52); // largest integer exactly representable in IEEE double
$v2 = $v1 - 0.4;
$v3 = $v1 + 0.4;
printf("%.0f\n", round($v2));
printf("%.0f\n", round($v3));
// Also test typical small numbers
printf("%.0f\n", round(2.5));
?>
--EXPECTF--
%d
%d
3
--CLEAN--
<?php
unset($v1, $v2, $v3);
?>
