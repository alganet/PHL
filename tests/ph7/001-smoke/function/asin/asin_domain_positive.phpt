--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
asin(2) should produce NaN
--FILE--
<?php
// NaN compares unequal to itself
$result = asin(2);
echo $result != $result ? "OK" : "FAIL:" . $result;
?>
--EXPECT--
OK
--CLEAN--
<?php
unset($result);
