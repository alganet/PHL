--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
bindec(): non-string-coercible argument throws TypeError (PHP 8)
--FILE--
<?php
foreach ([[], new stdClass] as $v) {
    try { bindec($v); } catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
}
?>
--EXPECT--
bindec(): Argument #1 ($binary_string) must be of type string, array given
bindec(): Argument #1 ($binary_string) must be of type string, stdClass given
--CLEAN--
<?php
