--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_replace overwrites values at matching numeric keys
--FILE--
<?php
$a = array(5 => 'a');
$b = array(5 => 'b');
$r = array_replace($a, $b);
echo $r[5];
?>
--EXPECT--
b
--CLEAN--
<?php
unset($a, $b, $r);
