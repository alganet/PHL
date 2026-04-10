--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: inside an array literal stops at closing bracket
--FILE--
<?php
$arr = [fn($x) => $x * 2, fn($x) => $x + 1];
echo $arr[0](5), "\n";
echo $arr[1](5), "\n";
?>
--EXPECT--
10
6
--CLEAN--
<?php
unset($arr);
