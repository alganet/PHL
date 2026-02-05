--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_numeric function
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
var_dump(is_numeric("42"));
var_dump(is_numeric(42));
var_dump(is_numeric(3.14));
var_dump(is_numeric("3.14"));
var_dump(is_numeric("hello"));
?>
--EXPECT--
bool(TRUE)
bool(TRUE)
bool(TRUE)
bool(TRUE)
bool(FALSE)
--CLEAN--
<?php

