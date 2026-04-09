--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nested symmetric array destructuring
--FILE--
<?php
[[$a, $b], $c] = [[1, 2], 3];
echo "$a $b $c\n";

[$x, [$y, $z]] = [10, [20, 30]];
echo "$x $y $z\n";
?>
--EXPECT--
1 2 3
10 20 30
--CLEAN--
<?php
unset($a, $b, $c, $x, $y, $z);
