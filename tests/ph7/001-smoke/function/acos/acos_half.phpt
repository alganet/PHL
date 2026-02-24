--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
acos(0.5) approximates pi/3
--FILE--
<?php
$result = acos(0.5);
$pi_third = 3.141592653589793 / 3;
if (abs($result - $pi_third) < 0.0000001) {
    echo "OK";
} else {
    echo "FAIL:" . $result;
}
?>
--EXPECT--
OK
--CLEAN--
<?php
unset($result, $pi_third);
