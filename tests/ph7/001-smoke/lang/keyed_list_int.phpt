--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Keyed list destructuring: explicit integer keys
--FILE--
<?php
[1 => $a, 0 => $b] = ["x", "y"];
echo "$a $b\n";
?>
--EXPECT--
y x
--CLEAN--
<?php
unset($a, $b);
