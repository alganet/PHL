--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array operations
--FILE--
<?php
$a = array(1, 2, 3);
echo $a[0]; // 1
echo "\n";
echo $a[1]; // 2
echo "\n";
echo count($a); // 3
echo "\n";
$a[] = 4;
echo count($a); // 4
?>
--EXPECT--
1
2
3
4
--CLEAN--
<?php
unset($a);
?>