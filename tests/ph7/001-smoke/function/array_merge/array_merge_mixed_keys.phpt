--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_merge handles mixed numeric and string keys
--FILE--
<?php
$a = array(5 => 'a', 'k' => 'va');
$b = array(5 => 'b', 'k' => 'vb');
$c = array_merge($a, $b);
echo count($c) . "\n";
echo $c[0] . ',' . $c[1] . ',' . $c['k'];
?>
--EXPECT--
3
a,b,vb
--CLEAN--
<?php
unset($a, $b, $c);
