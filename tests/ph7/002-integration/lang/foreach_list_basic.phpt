--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
foreach with list() unpacking
--FILE--
<?php
$rows = [
    [1, 2],
    [3, 4],
    [5, 6],
];
foreach ($rows as list($a, $b)) {
    echo "$a $b\n";
}
?>
--EXPECT--
1 2
3 4
5 6
--CLEAN--
<?php
