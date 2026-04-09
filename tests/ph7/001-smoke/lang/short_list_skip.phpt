--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Symmetric array destructuring with skipped elements
--FILE--
<?php
[, $b] = [1, 2];
echo "$b\n";

[$a, , $c] = [1, 2, 3];
echo "$a $c\n";

[, , $third] = [10, 20, 30];
echo "$third\n";
?>
--EXPECT--
2
1 3
30
--CLEAN--
<?php
unset($a, $b, $c, $third);
