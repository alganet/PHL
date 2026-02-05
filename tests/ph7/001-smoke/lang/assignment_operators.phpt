--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Assignment operators -= /= %= ^=
--FILE--
<?php
$a = 10;
$a -= 3;
echo "a -= 3: $a\n";

$b = 20;
$b /= 4;
echo "b /= 4: $b\n";

$c = 15;
$c %= 7;
echo "c %= 7: $c\n";

$d = 5;
$d ^= 3;
echo "d ^= 3: $d\n";
?>
--EXPECT--
a -= 3: 7
b /= 4: 5
c %= 7: 1
d ^= 3: 6
--CLEAN--
<?php
unset($a, $b, $c, $d);
