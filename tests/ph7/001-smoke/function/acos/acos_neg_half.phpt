--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
acos(-0.5) approximates 2pi/3
--FILE--
<?php
$result = acos(-0.5);
$two_pi_third = 2 * 3.141592653589793 / 3;
if (abs($result - $two_pi_third) < 0.0000001) {
    echo "OK";
} else {
    echo "FAIL:" . $result;
}
?>
--EXPECT--
OK
--CLEAN--
<?php
unset($result, $two_pi_third);
