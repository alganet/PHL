--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Keyed list destructuring: list() long form
--FILE--
<?php
list("a" => $x) = ["a" => 9];
echo "$x\n";
?>
--EXPECT--
9
--CLEAN--
<?php
unset($x);
