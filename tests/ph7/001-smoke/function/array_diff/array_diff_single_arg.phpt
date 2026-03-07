--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_diff with single array returns that array
--FILE--
<?php
$a = array(1, 2, 3);
$r = array_diff($a);
foreach ($r as $k => $v) echo "$k:$v,";
echo PHP_EOL;
?>
--EXPECT--
0:1,1:2,2:3,
--CLEAN--
<?php
unset($a, $r);
