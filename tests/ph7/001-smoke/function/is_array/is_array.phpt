--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_array function
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
var_dump(is_array(array()));
var_dump(is_array(42));
?>
--EXPECT--
bool(TRUE)
bool(FALSE)
--CLEAN--
<?php

