--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_key_exists: returns false for missing integer key
--FILE--
<?php
$a = array(0 => 'a', 5 => 'b');
echo array_key_exists(3, $a) ? 'true' : 'false';
echo PHP_EOL;
?>
--EXPECT--
false
--CLEAN--
<?php
unset($a);
