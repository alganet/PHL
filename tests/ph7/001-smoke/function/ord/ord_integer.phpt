--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ord(65) returns 54 with deprecation
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip';
--FILE--
<?php
echo ord(65) . "\n";
?>
--EXPECTF--
Error [%d]: ord(): Providing a string that is not one byte long is deprecated. Use ord($str[0]) instead in %s on line %d
54
--CLEAN--
<?php

