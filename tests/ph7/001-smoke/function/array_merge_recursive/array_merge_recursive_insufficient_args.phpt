--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_merge_recursive() with no arguments returns an empty array
--FILE--
<?php
$a = array_merge_recursive();
echo is_array($a) ? 'array' : gettype($a);
echo "\n";
echo count($a);
?>
--EXPECT--
array
0
--CLEAN--
<?php
unset($a);
