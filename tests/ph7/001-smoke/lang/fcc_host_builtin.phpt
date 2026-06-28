--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
First-class callable of a host builtin: strlen(...), array_map(strtoupper(...))
--FILE--
<?php
$len = strlen(...);
echo $len("hello"), "\n";
echo implode(",", array_map(strtoupper(...), ["a", "b", "c"])), "\n";
$up = strtoupper(...);
echo $up("php"), "\n";
?>
--EXPECT--
5
A,B,C
PHP
--CLEAN--
<?php
