--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: Syntax error - unexpected token ')'
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
echo ) 1;
?>
--EXPECTF--
%AParse error:%AUnmatched ')'%AParse error:%Asyntax error, unexpected token ";"%A
--CLEAN--
<?php

