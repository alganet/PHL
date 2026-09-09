--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sprintf with %c format specifier; a missing argument throws ArgumentCountError (PHP 8)
--FILE--
<?php
echo sprintf("%c", 65) === "A" ? "PASS_BASIC\n" : "FAIL_BASIC\n";
try {
    sprintf("%c");
    echo "NO_THROW\n";
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
PASS_BASIC
2 arguments are required, 1 given
--CLEAN--
<?php
