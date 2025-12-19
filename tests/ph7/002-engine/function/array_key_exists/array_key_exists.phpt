--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_key_exists basic test
--FILE--
<?php
$array = array('a' => 1, 'b' => 2, 0 => 3);
echo array_key_exists('a', $array) ? 'true' : 'false'; echo "\n";
echo array_key_exists('c', $array) ? 'true' : 'false'; echo "\n";
echo array_key_exists(0, $array) ? 'true' : 'false'; echo "\n";
?>
--EXPECT--
true
false
true