--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_merge with single array returns a copy
--FILE--
<?php
$a = array('x', 'y');
$b = array_merge($a);
echo $b[0] . ',' . $b[1];
?>
--EXPECT--
x,y
--CLEAN--
<?php
unset($a, $b);
