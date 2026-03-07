--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_values on empty array returns empty array
--FILE--
<?php
$v = array_values(array());
echo is_array($v) ? 'array' : 'not array';
echo "\n";
echo count($v);
?>
--EXPECT--
array
0
--CLEAN--
<?php
unset($v);
