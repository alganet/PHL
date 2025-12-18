--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_null function
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
var_dump(is_null(null));
var_dump(is_null(0));
var_dump(is_null(""));
?>
--EXPECT--
bool(TRUE)
bool(FALSE)
bool(TRUE)
