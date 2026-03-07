--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_merge with no arguments returns empty array
--FILE--
<?php
$r = array_merge();
echo is_array($r) ? 'array' : 'not array';
echo "\n";
echo count($r);
?>
--EXPECT--
array
0
--CLEAN--
<?php
unset($r);
