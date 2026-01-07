--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Not equal operator <> (alternative syntax)
--FILE--
<?php
if (1 <> 2) {
    echo "different\n";
} else {
    echo "same\n";
}
if (3 <> 3) {
    echo "different\n";
} else {
    echo "same\n";
}
?>
--EXPECT--
different
same