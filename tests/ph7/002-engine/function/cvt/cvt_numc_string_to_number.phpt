--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
String to number conversion in arithmetic (CVT_NUMC)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
$s = "10abc";
$r = $s + 5;
echo $r . "\n"; // 15

$s = "  -3.5e1foo"; // proper float style with leading space
echo ($s + 0) . "\n"; // numeric conversion to -35
?>
--EXPECT--
15
-35

--CLEAN--
<?php
unset($s, $r);
?>
