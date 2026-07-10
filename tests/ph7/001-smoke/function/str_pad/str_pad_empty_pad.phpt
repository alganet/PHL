--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_pad with an empty pad string throws ValueError only when padding is needed
--FILE--
<?php
// No padding required: the empty pad string is not validated.
echo str_pad("hello", 3, ""), "\n";
try {
    str_pad("x", 10, "");
    echo "NO_ERROR\n";
} catch (\ValueError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
hello
str_pad(): Argument #3 ($pad_string) must not be empty
--CLEAN--
<?php
