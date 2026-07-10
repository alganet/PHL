--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr_count with an empty needle throws ValueError
--FILE--
<?php
try {
    substr_count("abc", "");
    echo "NO_ERROR\n";
} catch (\ValueError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
substr_count(): Argument #2 ($needle) must not be empty
--CLEAN--
<?php
