--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Expected '(' after function name
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
function foo {}
?>
--EXPECTF--
%s 2 Error: Expected '(' after function name 'foo'
Compile error
--CLEAN--
<?php

