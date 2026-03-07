--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_diff preserves original keys from first array
--FILE--
<?php
$a = array(10 => 'a', 20 => 'b', 30 => 'c');
$b = array(1 => 'b');
$r = array_diff($a, $b);
foreach ($r as $k => $v) echo "$k:$v,";
echo PHP_EOL;
?>
--EXPECT--
10:a,30:c,
--CLEAN--
<?php
unset($a, $b, $r);
