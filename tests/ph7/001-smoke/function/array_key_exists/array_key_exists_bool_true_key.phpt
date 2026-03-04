--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_key_exists: bool true key matches integer 1
--FILE--
<?php
$a = array(1 => 'val');
echo array_key_exists(true, $a) ? 'true' : 'false';
echo PHP_EOL;
?>
--EXPECT--
true
--CLEAN--
<?php
unset($a);
