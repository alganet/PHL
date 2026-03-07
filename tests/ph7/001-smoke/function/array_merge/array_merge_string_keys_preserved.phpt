--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_merge preserves unique string keys from both arrays
--FILE--
<?php
$a = array('a' => 1);
$b = array('b' => 2);
$r = array_merge($a, $b);
echo $r['a'] . ',' . $r['b'];
?>
--EXPECT--
1,2
--CLEAN--
<?php
unset($a, $b, $r);
