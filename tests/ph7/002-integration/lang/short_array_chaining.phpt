--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Short array literal chaining and subscript on result
--FILE--
<?php
echo [10, 20, 30][0], "\n";
echo [10, 20, 30][2], "\n";
echo ['a' => 'x', 'b' => 'y']['b'], "\n";

$a = [1, 2] + [2 => 3];
echo count($a), "\n";

echo [[1,2],[3,4]][1][0], "\n";
?>
--EXPECT--
10
30
y
3
3
--CLEAN--
<?php
