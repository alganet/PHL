--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_merge_recursive() should merge values when key exists with NULL value
--FILE--
<?php
$b = array_merge_recursive(array('a' => null), array('a' => 2));
echo (is_null($b['a'][0]) ? 'NULL' : $b['a'][0]) . "\n";
echo $b['a'][1] . "\n";
?>
--EXPECT--
NULL
2
--CLEAN--
<?php
unset($b);
