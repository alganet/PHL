--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
COW: triple sharing isolates correctly
--FILE--
<?php
$a = [1, 2];
$b = $a;
$c = $a;
$b[0] = 99;
echo $a[0] . "\n";
echo $b[0] . "\n";
echo $c[0] . "\n";
?>
--EXPECT--
1
99
1
--CLEAN--
<?php
unset($a, $b, $c);
