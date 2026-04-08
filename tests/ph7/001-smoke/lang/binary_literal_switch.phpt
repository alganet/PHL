--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Binary literal in switch/case
--FILE--
<?php
switch (0b1010) {
    case 10:
        echo "matched decimal\n";
        break;
    default:
        echo "no match\n";
        break;
}

switch (10) {
    case 0b1010:
        echo "matched binary\n";
        break;
    default:
        echo "no match\n";
        break;
}

$x = 0b110;
switch ($x) {
    case 0b100:
        echo "four\n";
        break;
    case 0b101:
        echo "five\n";
        break;
    case 0b110:
        echo "six\n";
        break;
}
?>
--EXPECT--
matched decimal
matched binary
six
--CLEAN--
<?php

