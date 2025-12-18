--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_int function
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
var_dump(is_int(42));
var_dump(is_int(3.14));
var_dump(is_int("42"));
?>
--EXPECT--
bool(TRUE)
bool(FALSE)
bool(FALSE)
