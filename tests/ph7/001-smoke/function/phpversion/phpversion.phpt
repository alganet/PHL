--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
phpversion() reports the PHP-compat version and agrees with PHP_VERSION
--FILE--
<?php
// The no-arg form matches PHP_VERSION on every engine, so this is unskipped.
echo (phpversion() === PHP_VERSION) ? "matches\n" : "mismatch\n";
echo is_string(phpversion()) ? "string\n" : "notstring\n";
?>
--EXPECT--
matches
string
--CLEAN--
<?php
?>
