--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
func_get_arg called outside function context
--FILE--
<?php
// Test func_get_arg called outside function (should trigger error)
func_get_arg(0);
?>
--EXPECTF--
%s Warning: func_get_arg(): Called in the global scope
