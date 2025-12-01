--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
getrandmax returns an integer greater than 0
--FILE--
<?php
$r = getrandmax();
if (is_int($r) && $r > 0) echo "ok\n"; else echo "fail\n";
?>
--EXPECT--
ok
--CLEAN--
<?php
unset($r);
?>
