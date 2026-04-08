--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
atan with INF and NAN
--FILE--
<?php
echo atan(INF) . "\n";
echo atan(-INF) . "\n";
$r = atan(NAN);
echo is_float($r) && !($r == $r) ? "NAN_OK" : "NAN_FAIL";
echo "\n";
?>
--EXPECTF--
1.570796326794%s
-1.570796326794%s
NAN_OK
--CLEAN--
<?php
unset($r);
