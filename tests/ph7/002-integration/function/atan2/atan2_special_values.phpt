--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
atan2 with INF and NAN
--FILE--
<?php
echo atan2(INF, 1) . "\n";
echo atan2(-INF, 1) . "\n";
echo atan2(1, INF) . "\n";
echo atan2(1, -INF) . "\n";
echo atan2(INF, INF) . "\n";
$r = atan2(NAN, 1);
echo is_float($r) && !($r == $r) ? "NAN_OK" : "NAN_FAIL";
echo "\n";
?>
--EXPECTF--
1.570796326794%s
-1.570796326794%s
0
3.141592653589%s
0.7853981633974%s
NAN_OK
--CLEAN--
<?php
unset($r);
