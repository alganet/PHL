--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_key_exists: bool false key matches integer 0
--FILE--
<?php
$a = array(0 => 'val');
echo array_key_exists(false, $a) ? 'true' : 'false';
echo PHP_EOL;
?>
--EXPECT--
true
--CLEAN--
<?php
unset($a);
