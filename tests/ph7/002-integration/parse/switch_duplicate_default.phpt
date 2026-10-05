--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A second default clause in a switch is a compile-time fatal
--FILE--
<?php
// php refuses a second default clause when it compiles the switch, so
// nothing in the file runs.
echo "never printed\n";
switch (1) {
    case 1:
        echo "one\n";
    default:
        echo "first default\n";

    default:
        echo "second default\n";
}
--EXPECTF--
%AFatal error:%ASwitch statements may only contain one default clause in %s on line 11%A
