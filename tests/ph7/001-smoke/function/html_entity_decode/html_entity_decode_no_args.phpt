--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
html_entity_decode() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    html_entity_decode();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
html_entity_decode() expects at least 1 argument, 0 given
