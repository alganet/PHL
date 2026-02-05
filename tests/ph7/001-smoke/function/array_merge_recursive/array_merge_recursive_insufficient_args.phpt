--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_merge_recursive() with insufficient args
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Calling array_merge_recursive with no arguments should return NULL
var_dump(array_merge_recursive());
?>
--EXPECT--
null
--CLEAN--
<?php

