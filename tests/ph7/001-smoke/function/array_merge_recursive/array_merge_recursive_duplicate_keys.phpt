--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_merge_recursive() merges values with matching string keys into arrays
--FILE--
<?php
$b = array_merge_recursive(array('a' => 1), array('a' => 2));
echo $b['a'][0] . "\n";
echo $b['a'][1] . "\n";
?>
--EXPECT--
1
2
--CLEAN--
<?php
unset($b);
