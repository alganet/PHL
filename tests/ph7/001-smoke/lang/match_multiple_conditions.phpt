--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Match expression: multiple comma-separated conditions per arm
--FILE--
<?php
foreach ([1, 2, 3, 4, 5, 6, 7] as $n) {
    echo match ($n) {
        1, 2, 3 => 'low',
        4, 5, 6 => 'mid',
        7       => 'high',
    }, "\n";
}
?>
--EXPECT--
low
low
low
mid
mid
mid
high
--CLEAN--
<?php
