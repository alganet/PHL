--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
isset on array keys
--FILE--
<?php
$arr = array('key1' => 'value1', 'key2' => 'value2');
echo isset($arr['key1']) ? 'true' : 'false';
echo "\n";
echo isset($arr['nonexistent']) ? 'true' : 'false';
echo "\n";
?>
--EXPECT--
true
false