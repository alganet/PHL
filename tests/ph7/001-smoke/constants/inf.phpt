--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
INF constant should be a float and represent positive infinity
--FILE--
<?php
// constant must exist and be a float
echo gettype(INF) . "\n";
// INF should be greater than any finite number
echo INF > 1 ? "OK" : "FAIL";
?>
--EXPECT--
float
OK
--CLEAN--
<?php
?>
