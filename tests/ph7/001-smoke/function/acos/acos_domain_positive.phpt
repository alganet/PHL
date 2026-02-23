--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
acos(2) should produce NaN
--FILE--
<?php
// NaN compares unequal to itself
$result = acos(2);
echo $result != $result ? "OK" : "FAIL:" . $result;
?>
--EXPECT--
OK
--CLEAN--
<?php
unset($result);
?>