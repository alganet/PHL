--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_key_exists: returns false for empty array
--FILE--
<?php
$a = array();
echo array_key_exists('x', $a) ? 'true' : 'false';
echo PHP_EOL;
?>
--EXPECT--
false
--CLEAN--
<?php
unset($a);
