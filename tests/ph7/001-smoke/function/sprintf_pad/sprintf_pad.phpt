--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sprintf resets the custom pad character per specifier (no bleed between fields)
--FILE--
<?php
function sprintfPadCases(): array {
    return [
        sprintf("%'*10s|%10s", "a", "b"),
        sprintf("%'*10s|%-10s", "a", "b"),
        sprintf("%'x5s|%5s|%5s", "a", "b", "c"),
        sprintf("%05s|%5s", "a", "b"),
        sprintf("%'.10d|%10d", 1, 2),
        sprintf("%-'*10s|%10s", "a", "b"),
        sprintf("%1\$'*10s|%2\$10s", "a", "b"),
        sprintf("%'*8s|%08.2f|%8s", "a", 3.14, "b"),
    ];
}
echo implode("\n", sprintfPadCases()), "\n";
--EXPECT--
*********a|         b
*********a|b         
xxxxa|    b|    c
0000a|    b
.........1|         2
a*********|         b
*********a|         b
*******a|00003.14|       b
--CLEAN--
<?php
