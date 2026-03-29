--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Short array syntax: as function argument
--FILE--
<?php
echo count([1, 2, 3]), "\n";
echo implode(',', ['a', 'b', 'c']), "\n";
echo array_sum([10, 20, 30]), "\n";
?>
--EXPECT--
3
a,b,c
60
--CLEAN--
<?php
