--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
explode with empty delimiter throws ValueError
--FILE--
<?php
try {
    explode("", "test");
    echo "NO_ERROR\n";
} catch (\ValueError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
explode(): Argument #1 ($separator) must not be empty
--CLEAN--
<?php
