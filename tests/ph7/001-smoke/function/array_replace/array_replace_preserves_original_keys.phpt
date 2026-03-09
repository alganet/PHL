--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_replace preserves keys that only exist in the original array
--FILE--
<?php
$a = array('x' => 1, 'y' => 2);
$b = array('y' => 99);
$r = array_replace($a, $b);
echo $r['x'];
?>
--EXPECT--
1
--CLEAN--
<?php
unset($a, $b, $r);
