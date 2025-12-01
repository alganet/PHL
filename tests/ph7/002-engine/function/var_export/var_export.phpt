--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
var_export with return parameter produces a string representation
--FILE--
<?php
$a = array('x' => 1, 'y' => 2);
$s = var_export($a, true);
if ((stripos($s, 'array') !== false || stripos($s, 'Array') !== false) && stripos($s, 'x') !== false) echo "OK\n"; else echo "FAIL\n";
?>
--EXPECT--
OK

--CLEAN--
<?php
unset($a, $s);
?>
