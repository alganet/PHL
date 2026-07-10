--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_repeat with negative multiplier throws ValueError
--FILE--
<?php
try {
    str_repeat("a", -1);
    echo "NO_ERROR\n";
} catch (\ValueError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
str_repeat(): Argument #2 ($times) must be greater than or equal to 0
--CLEAN--
<?php
