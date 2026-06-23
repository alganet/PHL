--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Keyed list destructuring: duplicate keys assign same value
--FILE--
<?php
["a" => $x, "a" => $y] = ["a" => 1];
echo "$x $y\n";
?>
--EXPECT--
1 1
--CLEAN--
<?php
unset($x, $y);
