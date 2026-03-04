--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_key_exists: returns true for existing string key
--FILE--
<?php
$a = array('x' => 1, 'y' => 2);
echo array_key_exists('x', $a) ? 'true' : 'false';
echo PHP_EOL;
?>
--EXPECT--
true
--CLEAN--
<?php
unset($a);
