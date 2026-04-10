--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: used as array_map callback
--FILE--
<?php
$r = array_map(fn($x) => $x * 2, [1, 2, 3]);
foreach ($r as $v) echo $v, "\n";
?>
--EXPECT--
2
4
6
--CLEAN--
<?php
unset($r);
