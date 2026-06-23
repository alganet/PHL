--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Keyed list destructuring in foreach value target
--FILE--
<?php
$rows = [["id" => 1, "n" => "a"], ["id" => 2, "n" => "b"]];
foreach ($rows as ["id" => $id, "n" => $n]) {
    echo "$id $n\n";
}
?>
--EXPECT--
1 a
2 b
--CLEAN--
<?php
unset($rows, $id, $n);
