--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
compile with many numeric literals to stress memory
--FILE--
<?php
// Many numeric literals
$result = 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 +
          11 + 12 + 13 + 14 + 15 + 16 + 17 + 18 + 19 + 20 +
          21 + 22 + 23 + 24 + 25 + 26 + 27 + 28 + 29 + 30;
echo $result . "\n";
?>
--EXPECT--
465