--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
mt_rand returns an integer within mt_getrandmax
--FILE--
<?php
$m = mt_getrandmax();
$r = mt_rand();
if (is_int($r) && $r >= 0 && $r <= $m) echo "OK\n"; else echo "FAIL\n";
?>
--EXPECT--
OK

--CLEAN--
<?php
unset($m, $r);
?>
