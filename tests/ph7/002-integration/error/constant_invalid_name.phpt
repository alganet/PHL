--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
const: Invalid constant name
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
const 123 = 5;
?>
--EXPECTF--
%s 2 Error: const: Invalid constant name
Compile error
--CLEAN--
<?php

