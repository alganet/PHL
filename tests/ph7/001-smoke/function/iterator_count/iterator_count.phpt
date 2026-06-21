--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
iterator_count over a Generator and an array
--FILE--
<?php
function icGen() { yield 1; yield 2; yield 3; yield 4; }
echo iterator_count(icGen()), "\n";
echo iterator_count([10, 20, 30]), "\n";
function icEmpty() { if (false) { yield 1; } }
echo iterator_count(icEmpty()), "\n";
?>
--EXPECT--
4
3
0
--CLEAN--
<?php
