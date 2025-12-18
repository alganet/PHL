--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_string function
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
var_dump(is_string("hello"));
var_dump(is_string(42));
var_dump(is_string(array()));
?>
--EXPECT--
bool(TRUE)
bool(FALSE)
bool(FALSE)
