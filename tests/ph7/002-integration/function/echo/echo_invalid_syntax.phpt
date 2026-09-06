--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: echo with invalid syntax triggers unexpected token error
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
echo 1 2;
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected integer "2"%AParse error:%Asyntax error, unexpected token ";"%A
--CLEAN--
<?php

