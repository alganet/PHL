--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hexdec(): non-string-coercible argument throws TypeError (PHP 8)
--FILE--
<?php
foreach ([[], new stdClass] as $v) {
    try { hexdec($v); } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
}
?>
--EXPECT--
hexdec(): Argument #1 ($hex_string) must be of type string, array given
hexdec(): Argument #1 ($hex_string) must be of type string, stdClass given
--CLEAN--
<?php
