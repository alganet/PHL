--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Numeric literal separator in break/continue level arguments
--FILE--
<?php
// Underscored level argument on break/continue
for ($i = 0; $i < 3; $i++) {
    for ($j = 0; $j < 3; $j++) {
        if ($j === 1) {
            continue 0_2;
        }
        if ($i === 2 && $j === 0) {
            break 0_2;
        }
        echo "$i,$j\n";
    }
}
?>
--EXPECT--
0,0
1,0
--CLEAN--
<?php

