--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP: parse_ini_string with empty string
--FILE--
<?php
// Test empty string
$empty = parse_ini_string("");
echo $empty === array() ? "EMPTY_OK\n" : "EMPTY_FAIL\n";
?>
--EXPECT--
EMPTY_OK
--CLEAN--
<?php
unset($empty);
