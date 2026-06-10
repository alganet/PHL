--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP version constants are defined and internally consistent
--FILE--
<?php
// Presence and types (hold under any PHP-compat version, so unskipped).
echo defined('PHP_VERSION') ? "defined\n" : "missing\n";
echo preg_match('/^\d+\.\d+\.\d+/', PHP_VERSION) ? "format-ok\n" : "format-bad\n";
echo is_int(PHP_VERSION_ID) ? "id-int\n" : "id-notint\n";
echo (is_int(PHP_MAJOR_VERSION) && is_int(PHP_MINOR_VERSION)
      && is_int(PHP_RELEASE_VERSION)) ? "parts-int\n" : "parts-notint\n";
echo is_string(PHP_EXTRA_VERSION) ? "extra-string\n" : "extra-notstring\n";

// The identity PHP guarantees between the numeric pieces and the ID.
$expected = PHP_MAJOR_VERSION * 10000 + PHP_MINOR_VERSION * 100 + PHP_RELEASE_VERSION;
echo (PHP_VERSION_ID === $expected) ? "id-consistent\n" : "id-inconsistent\n";
?>
--EXPECT--
defined
format-ok
id-int
parts-int
extra-string
id-consistent
--CLEAN--
<?php
?>
