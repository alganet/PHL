--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array merge
--FILE--
<?php
$a = array(1, 2);
$b = array(3, 4);
$merged = array_merge($a, $b);
echo count($merged); // 4
echo "\n";
echo $merged[0] . $merged[1] . $merged[2] . $merged[3]; // 1234
?>
--EXPECT--
4
1234
--CLEAN--
<?php
unset($a, $b, $merged);
?>