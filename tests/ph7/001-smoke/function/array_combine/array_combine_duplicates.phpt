--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_combine with duplicate keys keeps last value for that key
--FILE--
<?php
$keys = array('x','x','y');
$vals = array(1,2,3);
$c = array_combine($keys, $vals);
// should map 'x' => 2 (last), 'y' => 3
echo $c['x'] . ',' . $c['y'] . PHP_EOL;
?>
--EXPECT--
2,3
--CLEAN--
<?php
unset($keys, $vals, $c);
