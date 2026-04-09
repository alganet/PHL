--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Symmetric array destructuring with fewer variables than values
--FILE--
<?php
[$a] = [1, 2, 3];
echo "$a\n";

[$x, $y] = [10, 20, 30];
echo "$x $y\n";
?>
--EXPECT--
1
10 20
--CLEAN--
<?php
unset($a, $x, $y);
