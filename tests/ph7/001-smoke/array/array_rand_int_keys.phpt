--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_rand returns integer keys
--FILE--
<?php
$array = array(10 => 'a', 20 => 'b', 30 => 'c');
$key = array_rand($array);
var_dump(is_int($key));
var_dump(in_array($key, array(10, 20, 30)));
?>
--EXPECT--
bool(true)
bool(true)
--CLEAN--
<?php
unset($array, $key);
