--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
invalid function and const names
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
function 123() {}
const 456 = 789;
echo "Should not reach here\n";
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected integer "123", expecting "("%AParse error:%Asyntax error, unexpected integer "456", expecting identifier%A
--CLEAN--
<?php

