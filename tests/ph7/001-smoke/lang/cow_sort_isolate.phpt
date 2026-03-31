--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
COW: sort does not affect copy
--FILE--
<?php
$a = [3, 1, 2];
$b = $a;
sort($a);
echo $b[0] . " " . $b[1] . " " . $b[2] . "\n";
echo $a[0] . " " . $a[1] . " " . $a[2] . "\n";
?>
--EXPECT--
3 1 2
1 2 3
--CLEAN--
<?php
unset($a, $b);
