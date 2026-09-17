--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
random_int accepts numeric strings as int arguments

--FILE--
<?php
$v = random_int("0", "10");
echo (is_int($v) && $v >= 0 && $v <= 10) ? "num_str_ok\n" : "num_str_fail\n";
?>
--EXPECT--
num_str_ok
--CLEAN--
<?php
