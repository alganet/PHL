--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Deeply nested symmetric array destructuring
--FILE--
<?php
[[$a, $b], [$c, $d]] = [[1, 2], [3, 4]];
echo "$a $b $c $d\n";

[$x, [[$y], $z]] = [10, [[20], 30]];
echo "$x $y $z\n";
?>
--EXPECT--
1 2 3 4
10 20 30
--CLEAN--
<?php
unset($a, $b, $c, $d, $x, $y, $z);
