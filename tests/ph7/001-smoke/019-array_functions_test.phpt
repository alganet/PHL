--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array functions
--FILE--
<?php
$array = array(1, 2, 3, 2);
echo in_array(2, $array) ? "yes" : "no"; // yes
echo "\n";
echo array_key_exists(0, $array) ? "yes" : "no"; // yes
echo "\n";
echo array_key_exists(4, $array) ? "yes" : "no"; // no
echo "\n";
$reversed = array_reverse($array);
echo $reversed[0]; // 2
echo "\n";
$unique = array_unique($array);
echo count($unique); // 3
?>
--EXPECT--
yes
yes
no
2
3
--CLEAN--
<?php
unset($array, $reversed, $unique);
?>