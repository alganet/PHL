--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
octdec(): non-string-coercible argument throws TypeError (PHP 8)
--FILE--
<?php
foreach ([[], new stdClass] as $v) {
    try { octdec($v); } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
}
?>
--EXPECT--
octdec(): Argument #1 ($octal_string) must be of type string, array given
octdec(): Argument #1 ($octal_string) must be of type string, stdClass given
--CLEAN--
<?php
