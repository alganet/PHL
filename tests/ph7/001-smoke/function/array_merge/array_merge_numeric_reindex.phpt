--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_merge reindexes numeric keys starting from zero
--FILE--
<?php
$a = array(5 => 'a');
$b = array(5 => 'b');
$c = array_merge($a, $b);
echo $c[0] . ',' . $c[1];
?>
--EXPECT--
a,b
--CLEAN--
<?php
unset($a, $b, $c);
