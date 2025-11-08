--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Var_dump and print_r
--FILE--
<?php
$array = array(1, 2, "three");
ob_start();
var_dump($array);
$var_dump_output = ob_get_clean();
echo strpos($var_dump_output, "array(3)") !== false ? "var_dump_ok" : "var_dump_fail";
echo "\n";
$print_r_output = print_r($array, true);
echo strpos($print_r_output, "three") !== false ? "print_r_ok" : "print_r_fail";
?>
--EXPECT--
var_dump_ok
print_r_ok
--CLEAN--
<?php
unset($array, $var_dump_output, $print_r_output);
?>