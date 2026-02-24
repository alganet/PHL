--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
acos(1) returns 0
--FILE--
<?php
$result = acos(1);
if (abs($result - 0.0) < 0.0000001) {
    echo "OK";
} else {
    echo "FAIL:" . $result;
}
?>
--EXPECT--
OK
--CLEAN--
<?php
unset($result);
