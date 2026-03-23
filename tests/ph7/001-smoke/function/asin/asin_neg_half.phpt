--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
asin(-0.5) approximates -pi/6
--FILE--
<?php
$result = asin(-0.5);
$neg_pi_sixth = -3.141592653589793 / 6;
if (abs($result - $neg_pi_sixth) < 0.0000001) {
    echo "OK";
} else {
    echo "FAIL:" . $result;
}
?>
--EXPECT--
OK
--CLEAN--
<?php
unset($result, $neg_pi_sixth);
