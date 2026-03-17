--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_uintersect should return the original array when only an array and a callback are provided
--FILE--
<?php
$a = array(0 => 'a', 1 => 'b');
$c = array_uintersect($a, function($x, $y) { return strcmp($x, $y); });
foreach ($c as $k => $v) {
    echo $k . ':' . $v;
}
?>
--EXPECT--
0:a1:b
--CLEAN--
<?php
unset($a, $c);
