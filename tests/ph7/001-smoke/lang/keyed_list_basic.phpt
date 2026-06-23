--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Keyed list destructuring: basic string keys
--FILE--
<?php
["a" => $x, "b" => $y] = ["a" => 1, "b" => 2];
echo "$x $y\n";
?>
--EXPECT--
1 2
--CLEAN--
<?php
unset($x, $y);
