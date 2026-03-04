--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
list() construct with invalid expression error
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// This should trigger a compile error: "list(): Expecting a variable not an expression"
list(1) = array(1);
?>
--EXPECTF--
%s %d Error:  list(): Expecting a variable not an expression
Compile error
--CLEAN--
<?php

