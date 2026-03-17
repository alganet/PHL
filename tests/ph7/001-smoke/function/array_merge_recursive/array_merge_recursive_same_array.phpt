--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_merge_recursive() called with the same array twice should merge values
--FILE--
<?php
$a = array('a' => 1);
$b = array_merge_recursive($a, $a);
echo $b['a'][0] . "\n";
echo $b['a'][1] . "\n";
?>
--EXPECT--
1
1
--CLEAN--
<?php
unset($a, $b);
