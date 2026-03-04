--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_key_exists: returns true for existing integer key
--FILE--
<?php
$a = array(0 => 'a', 5 => 'b');
echo array_key_exists(5, $a) ? 'true' : 'false';
echo PHP_EOL;
?>
--EXPECT--
true
--CLEAN--
<?php
unset($a);
