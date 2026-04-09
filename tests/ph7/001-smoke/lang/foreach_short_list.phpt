--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
foreach with symmetric array destructuring
--FILE--
<?php
$rows = [[1, 2], [3, 4], [5, 6]];
foreach ($rows as [$a, $b]) {
    echo "$a $b\n";
}
?>
--EXPECT--
1 2
3 4
5 6
--CLEAN--
<?php
unset($rows, $a, $b);
