--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Foreach loop
--FILE--
<?php
$array = array(1, 2, 3);
foreach ($array as $value) {
    echo $value;
}
echo "\n";
foreach ($array as $key => $value) {
    echo $key . ":" . $value . " ";
}
?>
--EXPECT--
123
0:1 1:2 2:3
--CLEAN--
<?php
unset($array, $value, $key);
?>