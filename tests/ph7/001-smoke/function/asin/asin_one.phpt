--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
asin(1) approximates pi/2
--FILE--
<?php
$result = asin(1);
$pi_half = 3.141592653589793 / 2;
if (abs($result - $pi_half) < 0.0000001) {
    echo "OK";
} else {
    echo "FAIL:" . $result;
}
?>
--EXPECT--
OK
--CLEAN--
<?php
unset($result, $pi_half);
